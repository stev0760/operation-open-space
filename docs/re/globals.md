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

- Private message 0x991 is the abort-to-intro request, 0x9a0 opens the Ship Watch, 0x9a5 reloads the menu bar, 0x9aa toggles the Icon Watch, 0x9b4 toggles the All Ships window `IS_SMART`, and 0x9f0 and 0x9fa carry the `DEMO` and `PLAY` command lines ([shell.md](shell.md#screen-states), [shell.md](shell.md#start-up)). [BYTES seg2:0x337 seg2:0x34a seg2:0x351 seg2:0x358 seg2:0x35f seg2:0x379 seg2:0x536 seg11:0x6c54 seg2:0x4e1 seg13:0x5878 seg2:0x4e9 seg2:0x16a2 seg2:0x4f0 seg2:0x513 seg13:0x638a seg2:0x86a seg6:0x1c3d seg6:0x1c63]
- The 0xa04 handler calls the music selector seg10:0x7480 with wParam. Its sender is [OPEN]: no listing carries 0xa04 as an operand. [BYTES seg2:0x386 seg2:0x4cc seg2:0x4cf]

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
  | 0x19c | Alt+B | when DGROUP:0x20 ≠ 0, sets the `WS_OVERLAPPEDWINDOW` bits (0x00CF0000) in the main window's style; then WM_SYSCOMMAND SC_MINIMIZE | [BYTES seg2:0x1665 seg2:0x1486 seg2:0x14a4 seg2:0x14b1] |
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
  - States 2–6, 8 and 9 are the wave debriefing, the wave briefing, GAME OVER, the high-score table, the ambulance visit, the Hall of Justice and the game-completion ending ([shell.md](shell.md#screen-states)). [BYTES seg9:0x6ec4 seg9:0x8a9b seg9:0x6a32 seg6:0x6322 seg16:0xd9b seg16:0x6ba2 seg16:0x73dc]
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
    | seg7:0x196a | 800, once, when an object outside pools 14, 6, 7 and 2 that the hit history of the last 200 frames names vanishes through a gate ([core.md](core.md#the-vanish-seg70x13b2-and-seg70x158e)) | [BYTES seg7:0x18cd seg7:0x18ed seg7:0x1923 seg7:0x196a] |
    | seg8:0x6785 | 3000 when the hit dispatcher destroys a virused icon credited to the player ([core.md](core.md#hit-dispatcher-seg80x5df2)) | [BYTES seg8:0x675d seg8:0x677d seg8:0x6785] |
    | seg8:0x7971 | 3000 when a player's gun shot destroys a virused icon ([core.md](core.md#shot-hit-test-seg80x3688-and-seg80x6f3c)) | [BYTES seg8:0x792f seg8:0x7971] |
    | seg8:0x67a6 | 1000 when the hit dispatcher destroys a pool-9 virus credited to the player | [BYTES seg8:0x6790 seg8:0x67a6] |
    | seg13:0x1b4a | 1000 (`add dword`) when a computer-flown teammate of the player captures an icon ([core.md](core.md#capturing-an-icon)) | [BYTES seg13:0x1b19 seg13:0x1b4a] |
    | seg8:0x92cd | 400 | [BYTES seg8:0x92cd] |
    | seg16:0x7228 | 50000 | [BYTES seg16:0x7228] |
    | seg13:0x1c32 | (DGROUP:0x9602 + 4) × 1000 when the player captures an icon, DGROUP:0x9602 being the defenders it takes over; 4000 without defenders ([core.md](core.md#capturing-an-icon)) | [BYTES seg13:0x1c1f seg13:0x1c25 seg13:0x1c2a seg13:0x1c32] [ORACLE wave-0001] |

  - What earns the other fixed amounts is [OPEN]. No instruction of the wave-end routines seg9:0x6bc2, seg9:0x6cc4, seg9:0x2828 or the debriefing seg9:0x6fee adds to the score. [BYTES seg9:0x6bc2 seg9:0x6cc4 seg9:0x2828 seg9:0x6fee]
- **DGROUP:0x5078 (dword), the rating.**
  - seg9:0x484c, run once per wave at the start of the wave generator ([core.md](core.md#scoring), The level), sets DGROUP:0x9334 to the number of entries in the 12-dword table at DGROUP:0x9bc that are below the rating, in a signed 32-bit compare. The table holds 26000, 40000, 55000, 70000, 90000, 120000, 150000, 200000, 280000, 400000, 550000 and 700000. [BYTES seg9:0x485b seg9:0x4870 seg9:0x4878 seg9:0x4882 seg9:0x4888 DGROUP:0x9bc]
  - The wave generator seg9:0x3202 reads and adjusts DGROUP:0x9334. [BYTES seg9:0x35bc seg9:0x3654 seg9:0x38fc seg9:0x39b6]
  - It changes at these sites:

    | Site | Change | Evidence |
    |---|---|---|
    | seg8:0x7c34 | + P[pool], capped at 500000 | [BYTES seg8:0x7c1a seg8:0x7c22 seg8:0x7c29 seg8:0x7c34] |
    | seg8:0x7c57, seg8:0x7c8c, seg8:0x7cad, seg8:0x7cd0 | the other kill-scoring terms (below) | [BYTES seg8:0x7c57 seg8:0x7c8c seg8:0x7cad seg8:0x7cd0] |
    | seg4:0x2ef0 | + race bonus | [BYTES seg4:0x2ef0] |
    | seg13:0x1b53 | + 1000, the teammate capture of seg13:0x1b4a | [BYTES seg13:0x1b53] |
    | seg13:0x1c3a | + the player's capture award of seg13:0x1c32 | [BYTES seg13:0x1c3a] |
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
| DGROUP:0x1a | 2 | BOOL | while non-zero the tick seg7:0x0 returns at once, input included ([shell.md](shell.md#exit)) [BYTES seg7:0x10 seg7:0x15] | seg2:0xe07, seg10:0x23c (1); seg2:0xe65, seg7:0x441, seg10:0x2e, seg11:0x7b5f, seg15:0x57fc (0) | seg7:0x10, seg7:0x868, seg7:0x9b3 |
| DGROUP:0x1c | 2 | BOOL | first timer tick pending [BYTES seg2:0x398 seg2:0x1205 seg2:0x120e] | seg2:0x398, seg2:0x120e | seg2:0x1205 |
| DGROUP:0x1e | 2 | BOOL | WM_PAINT handling enabled [BYTES seg2:0x123b seg2:0x7ac] | seg2:0x123b | seg2:0x7ac |
| DGROUP:0x58a | 2 | BOOL | redraw the player's sprite: while it is set (and DGROUP:0x951a and DGROUP:0x238c are 0) step 22 of the frame calls seg7:0x1208, which clears it ([world.md](world.md#update-loop)); the smoke mover sets it for a puff over the player's square ([world.md](world.md#smoke-seg70x5b68-and-seg70x5e04)) [BYTES seg7:0xd7d seg7:0x13a2 seg7:0x6025] | seg7:0x6025, seg12:0x4f3e (1); seg7:0x13a2 (0) | seg7:0xd7d |
| DGROUP:0x58c | 2 | BOOL | the command line changed: while it is set, DGROUP:0x951a = 0 and DGROUP:0x5bc < now, the frame calls seg7:0x1f38, which clears it ([core.md](core.md#messages-seg30x6ec)) [BYTES seg3:0x3397 seg7:0x7e1 seg7:0x7f3] | seg3:0x3397, seg7:0x1871 | seg7:0x7e1 |
| DGROUP:0x594, DGROUP:0x598 | 4 | int[2] | gun shot slots: 7 (seg11:0x4e0a stores it) and 2 more, together the 9 slots at DGROUP:0x6098 [BYTES seg11:0x4e19 DGROUP:0x598] | seg11:0x4e19 | seg11:0x5011, seg7:0x938 |
| DGROUP:0x5a0 | 2 | int | world width in 1/8-unit cells: 3200, so 25600 world units ([world.md](world.md#world-coordinates)) [BYTES DGROUP:0x5a0 seg8:0x1c27 seg8:0x1c2a] | none in INSPACE.EXE | seg8:0x1c27, seg10:0x75c, seg11:0x1e |
| DGROUP:0x5a2 | 2 | int | world height in 1/8-unit cells: 3200 [BYTES DGROUP:0x5a2 seg8:0x1c66] | none in INSPACE.EXE | seg8:0x1c66, seg10:0x77b, seg11:0x2c |
| DGROUP:0x5a8 | 4 | DWORD | play-time frame counter: each wave frame and each ambulance frame adds 1 ([timing.md](timing.md#game-time)). The save keeps it as header +0x18b, and the Restore Game preview shows it / 36 as the play time ([formats.md](formats.md#header)). Only seg6:0x8fac, seg6:0x9754, seg7:0x819, seg7:0xb2d and seg16:0x1f0c reference it, so no new game resets it in INSPACE.EXE; an oracle run with two new games is [OPEN] [BYTES seg6:0x8fac seg6:0x9754 seg7:0x819 seg7:0xb2d seg16:0x1f0c] | seg7:0xb2d, seg16:0x1f0c, seg6:0x9754 | seg6:0x8fac, seg7:0x819 |
| DGROUP:0x5b8 | 4 | DWORD ms | due time of the violence decay: when the player's `state` is 0 and the dword is below now (unsigned), the wave pass sets it to now + 10000 and calls seg7:0x2128 (DGROUP:0x9432 below); seg18:0x0 does the same in seg18 mode. It is 0 in the file and nothing else writes it, so the first decay comes on the first such pass and the 10-s phase runs on across waves [BYTES DGROUP:0x5b8 seg7:0x8d5-0x8ea seg7:0x905-0x922 seg18:0x203-0x257] | seg7:0x91a, seg18:0x250 | seg7:0x90d, seg18:0x243 |
| DGROUP:0x5bc | 4 | DWORD ms | the command line's due time that the frame gate compares; no instruction writes it by its address and it is 0 in the file, so the gate always passes. A write through a block copy or a pointer was not searched [OPEN] [BYTES seg7:0x7f3 DGROUP:0x5bc] | none by address | seg7:0x7f3 |
| DGROUP:0x5c0 | 4 | DWORD ms | written as now + 1200 next to every DGROUP:0x58c = 1 and read by no instruction by its address ([core.md](core.md#the-command-line-notice-seg30x332e)) [BYTES seg3:0x33a7 seg7:0x187e] | seg3:0x33a7, seg3:0x340e, seg3:0x3455, seg4:0x69af, seg7:0x187e, seg11:0x65a2, seg13:0x19bc, seg13:0x69dc | none by address |
| DGROUP:0x5cc | 2 | int | sub-step i of the shot-slot loop: seg7:0x95d adds 1 for each i; a slot whose dword +0x90 equals it changes velocity ([structs.md](structs.md#shot-slots-9--0x94-bytes-at-dgroup0x6098)) [BYTES seg7:0x95d seg8:0x3aec] | seg7:0x95d | seg8:0x3aec |
| DGROUP:0x928 | 2 | BOOL | the fast flag, the manual's Ctrl skip of the landing: set by Ctrl or QuickIntro in states 6 and 8 and by the seg18 entry, cleared by state-6 phase 64 and state-8 phase 10; it skips the corridor, the incoming jet and the descent ([shell.md](shell.md#screen-states), Ctrl skip) [BYTES seg16:0xd5e seg15:0x3192 seg15:0x3cb9] | seg16:0xd5e, seg16:0xd6b, seg16:0x6948, seg16:0x6955, seg18:0x114 (1); seg16:0x1bd9, seg16:0x6a7d (0) | seg15:0x1dab, seg15:0x3192, seg15:0x35f8, seg15:0x3876, seg15:0x38cc, seg15:0x3900, seg15:0x397b, seg15:0x3cb9 |
| DGROUP:0x93c | 2 | BOOL | 1 in the DGROUP image: the landing scene's end raises DGROUP:0x9004 by Calibrate at depth ≥ 8 once and clears it, so only the first landing of a session does ([timing.md](timing.md#adaptive-screen-threshold)) [BYTES DGROUP:0x93c seg15:0x3558-0x3581] | seg15:0x3581 (0) | seg15:0x355f |
| DGROUP:0x94e | 2 | int | race code from Navigation Enter: 4 × course + opponents + 1, 0 when the wave is no race ([nav.md](nav.md), [world.md](world.md#difficulty-inputs)) [BYTES seg6:0x3344 seg6:0x3488 seg9:0x3db6] | seg6:0x3344, seg6:0x3488, seg6:0x37e4 | seg9:0x3db6, seg11:0xc19, seg11:0x38ee |
| DGROUP:0x950 | 2 | int | duel code from Navigation Enter: opponents + 2, 0 when the wave is no duel [BYTES seg6:0x3527 seg9:0x3df9 seg9:0x3e06] | seg6:0x3527, seg6:0x37e7 | seg9:0x3df9, seg6:0x34ac |
| DGROUP:0x9bc | 48 | long[12] | rating thresholds, initialised data [BYTES DGROUP:0x9bc] | — | seg9:0x4870 |
| DGROUP:0xa6c | 4 | long | radar scale 100–500, 120 in the file; the HUD scrollbar shows 600 − scale ([world.md](world.md#scale-and-zoom), [timing.md](timing.md)) [BYTES DGROUP:0xa6c seg11:0xcf6 seg11:0xcf9] | seg11:0x7233, seg11:0x77b6, seg11:0x36c3 | seg11:0xcf9, seg11:0x3a99, seg11:0x3c50 |
| DGROUP:0xb0a | 2 | BOOL | while non-zero the ship damage handler plays bounce sound 7 instead of the design's hit sound ([core.md](core.md#damage-to-a-ship-seg80x45e0)). It is 0 in the DGROUP image and in every probe dump, and its only writer, seg8:0x4d43 at the end of a hit, stores 0, so it stays 0 [BYTES DGROUP:0xb0a seg8:0x494e seg8:0x4960 seg8:0x49bc seg8:0x49dc seg8:0x4d43] [ORACLE probe-0001] | seg8:0x4d43 (0) | seg8:0x494e, seg8:0x49bc |
| DGROUP:0xb0c | 2 | BOOL | 1 in the DGROUP image and the wave probe dump. The blast seg8:0x5ab0 sets it to 0 for its dispatch when the effect entry's frame counter is not 0, and to 1 before and after ([core.md](core.md#death-and-the-explosion)). The hit dispatcher's defender block and its messages 0x75 and 0x76 run only while it is not 0 ([core.md](core.md#hit-dispatcher-seg80x5df2)) [BYTES DGROUP:0xb0c seg8:0x5b88 seg8:0x5b99 seg8:0x5bb8 seg8:0x616a seg8:0x6589] [ORACLE probe-0001] | seg8:0x5b88, seg8:0x5bb8 (1); seg8:0x5b99 (0) | seg8:0x616a, seg8:0x6589 |
| DGROUP:0xb0e | 42 | int[21] | kill points per pool, initialised data [BYTES DGROUP:0xb0e] | — | seg8:0x7c11 |
| DGROUP:0xb38 | 42 | int[21] | rating weight per pool, initialised data [BYTES DGROUP:0xb38] | — | seg8:0x7c84 |
| DGROUP:0xb6a, DGROUP:0xb6c | 4 | int[2] | the effect brushes' colour offset and its step, 0 and 1 in the file; seg8:0x8256 adds the step every 61 ms or more and negates it outside 1 … 3, so the offset cycles 0 … 4 for the whole session ([world.md](world.md#effect-frame-seg80x3fbe), Pieces) [BYTES DGROUP:0xb6a seg8:0x82c6-0x82f4] | seg8:0x82e2, seg8:0x82f4 | seg8:0x42f9, seg8:0x84a6, seg8:0x8511, seg8:0x8526, seg8:0x88d4, seg8:0x8dbc, seg8:0x8dd1, seg13:0x5cc0 |
| DGROUP:0xf00 | 91 | byte[91] | sine × 128 for 0–90°, initialised data; the bytes differ from round(sin × 128) at 57 entries ([world.md](world.md#helpers)) [BYTES DGROUP:0xf00 seg6:0x71d3] | — | seg6:0x71d3, seg6:0x71f2 |
| DGROUP:0x1018 | 25 | char[5][5] | joystick direction map: target heading 0–15, or −1 at the centre ([world.md](world.md#joystick)) [BYTES DGROUP:0x1018 seg11:0x7570] | — | seg11:0x7570 |
| DGROUP:0x1042 | 2 | BOOL | the gun fires while it is non-zero, as for Shift or Space; its writer is [OPEN] [BYTES seg11:0x4df4] | — | seg11:0x4df4 |
| DGROUP:0x104c | 2 | BOOL | while non-zero the player's ship takes no damage; a restore clears it [BYTES seg8:0x4607 seg8:0x4614 seg6:0x98be] | seg6:0x98be (0) | seg8:0x4607 |
| DGROUP:0x1050 | 64 | int[16][2] | heading vectors (dx, dy), entry 0 = (0, −16) pointing up, clockwise in 22.5° steps ([world.md](world.md#heading-table)) [BYTES DGROUP:0x1050] | — | seg4:0x22a8, seg8:0x2f76 |
| DGROUP:0x111e | 2 | BOOL | ambulance busy: 1 while 1 ≤ DGROUP:0x9e49 < 6 ([core.md](core.md#the-ambulance)); no INSPACE.EXE instruction reads it [BYTES seg16:0xd4 seg16:0xea] | seg16:0xe2, seg16:0xea, seg16:0x337, seg16:0x5b2, seg16:0x620, seg16:0x8cd, seg16:0x118a, seg16:0x32f8 | none |
| DGROUP:0x1318 | 2 | int | saved as header +0x173 ([formats.md](formats.md#header)) [BYTES seg6:0x92aa seg6:0x9856]. The blast seg8:0x5ab0 does nothing while it is above 6, and a kind-2 pod's touch sets it to 1 ([core.md](core.md#death-and-the-explosion), [core.md](core.md#other-touches)) [BYTES seg8:0x5aec seg13:0x1d81]; seg18 reads and writes it often | seg6:0x9856, seg13:0x1d81, seg11:0x7406, seg14:0x435, seg18 | seg6:0x92aa, seg8:0x5aec, seg11:0xaa1d, seg18 |
| DGROUP:0x131a | 2 | BOOL | debug traces reach OutputDebugString; INI key `Aux` [BYTES seg6:0x300 seg13:0x4ed4] | seg6:0x300 | seg13:0x4ed4 |
| DGROUP:0x13dc | 2 | HDC | frame DC while GameFrame runs [BYTES seg2:0x1c7e seg2:0x1d44] | seg2:0x1c7e, seg2:0x1d44 | seg12:0x27e6 |
| DGROUP:0x13de | 2 | BOOL | state change pending: GameFrame calls seg2:0x1d58 instead of the tick [BYTES seg2:0x1c64 seg2:0x1db0 seg7:0xdd seg7:0x3fc] | seg7:0xdd, seg7:0x3fc, seg7:0x447, seg2:0x1db0 | seg2:0x1c64 |
| DGROUP:0x1858 | 16 | int[8] | per-ship auto-repair countdown: it falls by 1 on each repair tick, and at 0 or below that tick reloads it to max(1, 9 − `mass` / 75) instead of repairing ([core.md](core.md#auto-repair-seg110x1d7c)) [BYTES seg11:0x1e6f seg11:0x1e75 seg11:0x1e97 seg11:0x1eab] | seg11:0x1e6f, seg11:0x1e97 | seg11:0x1eab |
| DGROUP:0x1c3c | 2 | BOOL | the champions table has been drawn; seg6:0x5f7a(1), the repaint's call, clears it so the table is drawn again ([shell.md](shell.md#screen-states), State 5) [BYTES seg6:0x5f8d seg6:0x6235-0x6260] | seg6:0x5f7a | seg6:0x5f7a |
| DGROUP:0x1c42 | 2 | int | the served ship's heading on the pickup frames of the ambulance's state 1, −1 at the stop ([core.md](core.md#the-ambulance)) [BYTES seg16:0x302 seg16:0x498-0x4af seg16:0x3ee] | seg16:0xa2 | seg16:0xa2 |
| DGROUP:0x1d38 | 4 | LPARAM | pending input lParam [BYTES seg2:0x799] | seg2:0x799 | seg7:0x10c, seg18:0x1ef |
| DGROUP:0x1d3c | 4 | DWORD ms | start time [BYTES seg2:0xaa] | seg2:0xaa | seg2:0x721, seg2:0x126e |
| DGROUP:0x1d48 | 4 | POINT | cursor position saved at start [BYTES seg2:0x55 seg2:0x6c1] | seg2:0x55 | seg2:0x6c1, seg2:0x12a4 |
| DGROUP:0x1d60 | string | char[] | command line [BYTES seg2:0x24] | seg2:0x24 | seg6:0x1c1d |
| DGROUP:0x1df8 | 84 | far ptr[21] | object pools ([structs.md](structs.md#pool-table)) [BYTES seg6:0x1b8d seg18:0x1dfb] | seg6:0x1b8d, seg18:0x1dfb, seg7:0x5314 | seg18:0x20f, seg11:0x372b |
| DGROUP:0x1e30 | 4 | far ptr | pool 14 entry of that table. Code in most segments reads it directly instead of through DGROUP:0x9324 [BYTES seg4:0x8b95 seg7:0x11dc] | — | seg4:0x8b95, seg7:0x11dc |
| DGROUP:0x1e34 | 4 | far ptr | pool 15 entry, read only by seg18 ([structs.md](structs.md#pool-roles)) [BYTES seg18:0xe81] | — | seg18:0xe81 |
| DGROUP:0x1e4c | 4 | far ptr | locked 8-byte block, handle DGROUP:0x912a: the joystick reading, x word, y word and four button bytes ([world.md](world.md#joystick)) [BYTES seg6:0x1bda seg6:0x1bec] | seg6:0x1bec | — |
| DGROUP:0x1e50 | 4 | DWORD ms | tick clock, GetCurrentTime at each tick ([timing.md](timing.md#per-pass-work-in-seg70x0)) [BYTES seg7:0x24] | seg7:0x24 | seg2:0x758, seg12:0x2ab4 |
| DGROUP:0x1e58 | 4 | long | score ([Scoring](#scoring)) | seg8:0x7c3b, seg9:0xc4e, seg6:0x968a | seg11:0x4f7 |
| DGROUP:0x1e68 | 4 | DWORD | wave frame counter ([timing.md](timing.md#game-time)); seg9:0x6624 resets it to 0 and the ambulance frame also counts it [BYTES seg7:0xb32 seg4:0x2e6e seg9:0x6689 seg16:0x1f11] | seg7:0xb32, seg9:0x6689, seg16:0x1f11 | seg4:0x2e6e, seg11:0x1d92, seg11:0x4e6f |
| DGROUP:0x1e80 | 4 | DWORD ms | due time that the gameplay frame acts on; set when the player's ship is destroyed, and to now + 1000 when it leaves through the exit gate ([core.md](core.md#the-tick-takes-it-up-seg70x610)) [BYTES seg7:0x610 seg7:0x622 seg12:0x2adf seg16:0xae6 seg12:0x1e3b] | seg12:0x2adf, seg16:0xae6, seg12:0x1e3b | seg7:0x622 |
| DGROUP:0x1e84 | 4 | DWORD ms | as DGROUP:0x1e80, used instead when DGROUP:0x95bc ≠ 0 [BYTES seg12:0x2ac1 seg16:0xacd seg7:0x6b8] | seg12:0x2ac1, seg16:0xacd | seg7:0x6b8, seg7:0x1d1 |
| DGROUP:0x1e88 | 4 | DWORD ms | due time that leads to state 6, or to seg16:0x32a0 for a ship other than the player; the ambulance's states 3 and 4 rewrite it on every frame until its vanish ends ([core.md](core.md#the-ambulance)) [BYTES seg7:0x5ce seg7:0x5da seg16:0x568] | seg16:0x568; seg7:0x5e1, seg9:0xda4, seg9:0x5d58 (0) | seg7:0x5da |
| DGROUP:0x1e90 | 900 | struct[45] | effect table ([structs.md](structs.md#effect-table-45--0x14-bytes-at-dgroup0x1e90)) [BYTES seg18:0x9b6] | seg12:0x28d8, seg18:0x9b6 | seg7:0x129a |
| DGROUP:0x238c, DGROUP:0x238e | 4 | int[2] | view-shake velocity x and y; the shake runs while DGROUP:0x238c ≠ 0. seg11:0x9daa starts it with 9 and 6 ([core.md](core.md#the-shock-seg110x9daa)) [BYTES seg11:0x9db8 seg11:0x9de6 seg8:0x2b2 seg8:0x2dd seg8:0x360] | seg11:0x9de6, seg8:0x2b2, seg8:0x2dd, seg8:0x360 | seg8:0x0, seg11:0x9db8 |
| DGROUP:0x2390, DGROUP:0x2392 | 4 | int[2] | view-shake offset x and y in pixels; 8 × offset is added to the view origin [BYTES seg8:0x289 seg8:0x29a seg8:0x2e9 seg8:0x2f3] | seg8:0x289, seg8:0x29a | seg8:0x2e9 |
| DGROUP:0x2400 | 8 | int[4] | view rectangle in world units: origin x, y (DGROUP:0x2400, DGROUP:0x2402) and far corner (DGROUP:0x2404, DGROUP:0x2406) ([world.md](world.md#view)) [BYTES seg8:0x4f seg8:0x5f] | seg8:0x4f, seg8:0x5f | seg8:0x3b3, seg11:0x3469 |
| DGROUP:0x2408 | 4 | DWORD ms | end time of the view shake, now + 2000 [BYTES seg11:0x9de2 seg8:0x35c seg8:0x367] | seg11:0x9de2 | seg8:0x35c |
| DGROUP:0x2414 | 0xaf0 | struct[100] | effect pieces, 25 per effect entry 0 … 3 ([structs.md](structs.md#effect-pieces-4--25--0x1c-bytes-at-dgroup0x2414)) [BYTES seg8:0x430e-0x431b] | seg7:0x342e, seg8:0x9694, seg8:0x7d2, seg8:0x3fbe | the same |
| DGROUP:0x2f54 | 2240 | struct[28] | weapon records: 28 blocks of 0x50 bytes, filled by seg10:0x1fb2 up to DGROUP:0x380b; +0 is the name id 186 + w and +0x36 (DGROUP:0x2f8a) the Standard-edition replacement ([structs.md](structs.md#weapon-records-28--0x50-bytes-at-dgroup0x2f54), [shell.md](shell.md#intro-and-title-screen), **Plus content**) [BYTES seg10:0x1fbc seg10:0x1fb2 seg10:0x1fe8] | seg10:0x1fb2 | seg14:0x33a, seg14:0xfb7, seg14:0x16e5, seg16:0x3d66, seg16:0x5169 |
| DGROUP:0x38c8 | string | char[] | directory of the current wave, shown by the briefing ([shell.md](shell.md#game-start-dialogs), **Enter**) [BYTES seg6:0x3044 seg9:0x8b20] | lstrcpy at seg6:0x3044, seg6:0x30ed, seg6:0x348d, seg6:0x3642; seg11:0x4249 | seg9:0x8b20, seg17:0x3463, seg11:0x6ba8 |
| DGROUP:0x3b94 | 4 | DWORD ms | input is ignored until DGROUP:0x1e50 reaches it [BYTES seg2:0x75c] | — | seg2:0x75c |
| DGROUP:0x3c8c … DGROUP:0x3c9b | 16 | POINT[4] | the launch pad's trapezoid P0 … P3 for Polygon; DGROUP:0x3c96 and DGROUP:0x3c9a hold its height pc ([structs.md](structs.md#visit-and-launch-tables)) [BYTES seg15:0x4876-0x48c2 seg15:0x4a91] | seg15:0x4890, seg15:0x4898, seg15:0x489f, seg15:0x48a3, seg15:0x48a7, seg15:0x48bf, seg15:0x48c2, seg15:0x48f7 | seg15:0x4a2e (the Polygon at seg15:0x4a91); DGROUP:0x3c9a: seg15:0x0, seg15:0x374c, seg15:0x482e, seg15:0x4a2e, seg15:0x4c76, seg16:0x2144 |
| DGROUP:0x3c9c, DGROUP:0x3c9e | 4 | int[2] | the pad's x on the 4:3 play width, half(q) − half(pw), and 0 ([shell.md](shell.md#game-start-dialogs), item 7) [BYTES seg15:0x48e6-0x48fb] | seg15:0x48f2, seg15:0x48fb | none by address |
| DGROUP:0x3ca0 | 2 | int | the x of the crossing of the pad's two sloped edges; read by no instruction [BYTES seg15:0x4902-0x4950] | seg15:0x4950 | none |
| DGROUP:0x3ca2 | 2 | int | the y of that crossing on the screen, the vanishing point that the landing room's drawing seg15:0x454c uses (321 at 1024 × 768) [BYTES seg15:0x4953-0x4975 seg15:0x4792] | seg15:0x4975 | seg15:0x45a5, seg15:0x45d6, seg15:0x4792, seg15:0x47bc, seg15:0x47e4, seg15:0x480a |
| DGROUP:0x3ce4 | 2 | int | the pad's top width A = trunc(k/22) + 139 (131 at 1024 × 768); only seg15:0x482e's polygon reads it, from a register [BYTES seg15:0x484b-0x485b] | seg15:0x485b | none by address |
| DGROUP:0x3ce6 | 2 | int | the pad's width pw = trunc(k/36) + 164 (159 at 1024 × 768) [BYTES seg15:0x485e-0x486d] | seg15:0x486d | seg15:0x0, seg15:0x374c, seg15:0x482e, seg15:0x4a2e, seg15:0x4c76, seg16:0xdc1, seg16:0x1f78, seg16:0x2144 |
| DGROUP:0x4fd0 | 48 | int[8][3] | saved as header +0xe6; its meaning is [OPEN] [BYTES seg6:0x920b-0x920f seg6:0x97ba-0x97bf] | seg6:0x930a | seg6:0x8eea |
| DGROUP:0x5014 | 4 | DWORD | frame deadline of the auto-repair tick: once DGROUP:0x1e68 is past it, the tick runs and sets it to DGROUP:0x1e68 + 180, so it runs every 181 frames [BYTES seg11:0x1d92 seg11:0x1d97 seg11:0x1d9b seg11:0x1da1] | seg11:0x1da1 | seg11:0x1d92 |
| DGROUP:0x5018 | 4 | long | score as last drawn; −1 forces a redraw [BYTES seg11:0x508 seg9:0xcdd] | seg11:0x508, seg9:0xcdd | seg11:0x4ff |
| DGROUP:0x502c | 4 | DWORD ms | the low-fuel message timer: seg11:0x978 tests it once per change and sets now + 30000 at each message ([world.md](world.md#fuel-gauge)) [BYTES seg11:0x9bc seg11:0xa03] | seg11:0x978 | seg11:0x978 |
| DGROUP:0x5074 | 4 | DWORD ms | the fuel tick's deadline, now + 1000 ([world.md](world.md#mass-thrust-and-drag)) [BYTES seg11:0x1c3e seg11:0x1c47] | seg11:0x1c47 | seg11:0x1c3e |
| DGROUP:0x5078 | 4 | long | rating ([Scoring](#scoring)); no display draws it: seg3, seg11 and seg13 reference it only in the capture adds [BYTES seg13:0x1b53 seg13:0x1c3a] | stores seg9:0xc52, seg6:0x969b, seg16:0xb19, seg16:0x34e0, seg8:0x4736, seg8:0x7c34; adds seg4:0x2ef0, seg8:0x46f4, seg8:0x476c, seg13:0x1b53, seg13:0x1c3a | seg8:0x46c9, seg8:0x46fc, seg8:0x4708, seg8:0x471a, seg8:0x472e, seg8:0x4747, seg8:0x7c1a, seg9:0x4863, seg16:0xafb, seg16:0xb06, seg16:0x34c3, seg6:0x8f9b |
| DGROUP:0x5148, DGROUP:0x5160 | 16 | RECT[2] | the pair finder's rectangles: DGROUP:0x5148 the search area around the caller, DGROUP:0x5160 the caller's square ([core.md](core.md#pair-test-seg80x6afa)) [BYTES seg8:0x5224 seg8:0x5253 seg8:0x525c seg8:0x5294 seg8:0x6b7d seg8:0x6bb7] | seg8:0x5224, seg8:0x525c | seg8:0x6b7d, seg8:0x6bb7 |
| DGROUP:0x52dc … DGROUP:0x52ea | 16 | int[8] | collision party slots: velocity of B (DGROUP:0x52dc, 0x52de) and of A (DGROUP:0x52e0, 0x52e2), screen position of A (DGROUP:0x52e4, 0x52e6) and of B (DGROUP:0x52e8, 0x52ea), passed by reference to ISPACE.DLL ordinal 29 ([core.md](core.md#response-seg80x546c)) [BYTES seg8:0x6dd0 seg8:0x6e5e seg8:0x57a3 seg8:0x57db] | seg8:0x6dd0, seg8:0x6e5e, ISPACE.DLL ordinal 29 | seg8:0x546c |
| DGROUP:0x52fe | 0x24 × n | struct[] | defender entries, one per pool-2 record ([structs.md](structs.md#other-wave-tables)) [BYTES seg13:0x1b5e seg13:0x1b6e] | seg13:0x1b8c, seg13:0x1b93 | seg13:0x1b79, seg3:0x9b |
| DGROUP:0x576c | 40 | HBRUSH[20] | the effect brushes, made by seg10:0x4b2e with CreateSolidBrush ([world.md](world.md#effect-frame-seg80x3fbe), Pieces) [BYTES seg10:0x4c0b-0x4d16] | seg10:0x4c0b … seg10:0x4d16 | seg8:0x42f5, seg13:0x5cc0 |
| DGROUP:0x579c | 4 | DWORD ms | time of the colour offset's last step [BYTES seg8:0x82d0 seg8:0x82db] | seg8:0x82db | seg8:0x82d0 |
| DGROUP:0x57a8 | 0x23 × 8 | struct[8] | icon entries, one per pool-0 record ([structs.md](structs.md#other-wave-tables)) [BYTES seg7:0x5b07 seg9:0xb43] | seg9:0xb43, seg7:0x5b07 | seg7:0x234f, seg7:0x5a91 |
| DGROUP:0x58bc | 0x14 × 20 | struct[20] | smoke records ([structs.md](structs.md#smoke-records-20--0x14-bytes-at-dgroup0x58bc)); the placement writes −1 into the counters +0x10 (DGROUP:0x58cc) [BYTES seg8:0x1258-0x1280 seg9:0x6747-0x675d] | seg7:0x5b68, seg7:0x5d6a, seg7:0x5e04, seg8:0x124a, seg9:0x6750 | seg7:0x5e04, seg8:0x124a |
| DGROUP:0x5a54 | 0x14 × n | struct[] | last object hit by each pool-5 record ([structs.md](structs.md#other-wave-tables)) [BYTES seg8:0x54b4 seg8:0x55b3] | seg8:0x55ac | seg8:0x54b4 |
| DGROUP:0x5e98, DGROUP:0x5ea0 | 16 | HDC[4], HGDIOBJ[4] | the piece source of effect entries 0 … 3 and, for a pool-1 source, the object its DC replaced ([core.md](core.md#the-break-up-seg70x2e02)) [BYTES seg7:0x3015-0x306e] | seg7:0x2e02, seg8:0x3fbe, seg10:0x22e | seg8:0x44cf-0x4517 |
| DGROUP:0x6098 | 0x534 | struct[9] | gun shot slots of 0x94 bytes ([structs.md](structs.md#shot-slots-9--0x94-bytes-at-dgroup0x6098)) [BYTES seg8:0x3aec seg11:0x4e0a] | seg11:0x55ff | seg8:0x3aec |
| DGROUP:0x65cc | 0x20 × n | struct[] | link table of seg11:0x947a (GRAPPLING HOOK, HANDCUFFS); seg11:0x947a writes words at DGROUP:0x65d6 + 0x20k [BYTES seg11:0x947a seg12:0x3778] | seg11:0x947a | seg12:0x3778 |
| DGROUP:0x6678 | 0x1c × n | struct[] | one entry per gate, DGROUP:0x9318 entries, whose first four words are the gate's beam rectangle ([world.md](world.md#gates)); a gun shot whose head reaches the rectangle (+0, +2 − 5, +4, +6 + 5) of an entry ends ([core.md](core.md#shot-hit-test-seg80x3688-and-seg80x6f3c)) [BYTES seg8:0x37d7-0x37f3] | — | seg8:0x3688 |
| DGROUP:0x66e0 | 0x1c × 10 | struct[] | missile entries, one per pool-6 record; word +0x04 (DGROUP:0x66e4) is the weapon ([structs.md](structs.md#missile-entries-0x1c-bytes-at-dgroup0x66e0)) [BYTES seg8:0x6c67 seg11:0x7c12] | seg11:0x7c12, seg12:0x0 | seg8:0x6c67, seg12:0x0 |
| DGROUP:0x698c | 4 | DWORD ms | the fuel tanks' respawn time: seg12:0x4f4e respawns a pending tank only once now is past it ([core.md](core.md#other-touches)) [BYTES seg12:0x5068 seg12:0x5074] | seg12:0x279a, seg12:0x514e, seg13:0x1ce8 | seg12:0x5068 |
| DGROUP:0x6990 | 100 | char[100] | path of the selected Navigation row; the INI Track read may write 109 bytes into it ([nav.md](nav.md)) [BYTES seg6:0x3c81 seg6:0x3c84] | lstrcpy calls; GetPrivateProfileString at seg6:0x3c84 | seg6:0x5524, seg6:0x301a |
| DGROUP:0x69f4 | 4 | far ptr | the directory records, 0x297 bytes each; record DGROUP:0x94a2 belongs to the current wave ([nav.md](nav.md)) [BYTES seg6:0x3721 seg9:0x4663 seg9:0x4669] | seg6:0x3721, seg6:0x376e | seg9:0x4669, seg9:0x450e |
| DGROUP:0x6af8 | 4 | far ptr | the kept score list of 0x31-byte entries, handle DGROUP:0x94de, count DGROUP:0x94c6; while it is non-null seg6:0x6576 skips its scan ([formats.md](formats.md#score-list)) [BYTES seg6:0x658b seg6:0x717a] | seg6:0x6576, seg6:0x717a | seg6:0x658b |
| DGROUP:0x6b00 | 4 | DWORD ms | the time from which state 5 draws the champions table ([shell.md](shell.md#screen-states), State 5) [BYTES seg6:0x5e49-0x5f39 seg6:0x6235] | seg6:0x5d58 | seg6:0x5f7a |
| DGROUP:0x6b04 | 4 | far ptr | the champions table, 10 slots of 0x2e bytes in a 0x1cc-byte block, handle DGROUP:0x94d8 ([formats.md](formats.md#score-list)) [BYTES seg6:0x5ddc-0x5df9] | seg6:0x5d58 | seg6:0x62b6 |
| DGROUP:0x6b10 | 48 | int[8][3] | saved as header +0x116; its meaning is [OPEN] [BYTES seg6:0x9214-0x9218 seg6:0x97c3-0x97c8] | seg6:0x930a | seg6:0x8eea |
| DGROUP:0x6b40 | 16 | int[8] | saved as header +0x146; Restore Game prints their sum as the ships destroyed (string 3027, [formats.md](formats.md#the-headers-readers)) [BYTES seg6:0x922b-0x9236 seg10:0xa18a] | seg6:0x930a | seg6:0x8eea |
| DGROUP:0x6da8 | 4 | DWORD ms | the ambulance's re-entry time: now + 3000 at visit phase 64, 0 when the ship is back or the ambulance parks ([core.md](core.md#into-the-visit-and-back)) [BYTES seg16:0x1bfb-0x1c2b seg16:0x3332 seg16:0x8aa-0x8d3 seg7:0xc73-0xc8f] | seg16:0x1c2b, seg16:0x3332, seg16:0x8d3, seg7:0xe32, seg9:0x694d | seg7:0xc73, seg7:0xdfb |
| DGROUP:0x6e10 | 4 | DWORD ms | the ambulance mover's deadline: now + 750 at the pickup, now + 1300 at state 3, and now + 1100 at the state-3 → 4 step of the ship's part seg13:0x3748 ([core.md](core.md#the-ambulance)) [BYTES seg16:0x47c seg16:0x4d7 seg13:0x38e5] | seg16:0x47c, seg16:0x4d7, seg13:0x38e5 | seg16:0x4b6, seg16:0x517 |
| DGROUP:0x708c | 4 | DWORD | the dword 4 bytes below ship record 0, where a missile's turn writes now − 1 through seg13:0x6716(−1) ([core.md](core.md#the-turn-seg130xd6c)); no instruction names the address, and what else lives there is [OPEN] [BYTES seg13:0x673d seg13:0x6753] | seg13:0x674f (ship −1) | — |
| DGROUP:0x7090 | 0xad0 | struct[8] | ship records ([structs.md](structs.md#ship-records-8--0x15a-bytes-at-dgroup0x7090)) [BYTES seg4:0x7084] | seg4:0x7091 | seg4:0x7084 |
| DGROUP:0x7b60 | 0x100 × designs | int[][64][2] | gun muzzle offsets per design: 64 (x, y) pairs, entries 0–15 for one barrel by heading, 16–31 for a rear launch, 32–63 for two barrels ([core.md](core.md#the-muzzle-tables-seg100x1460-seg100x1620-seg100x1502)) [BYTES seg11:0x530b seg11:0x5314 seg11:0x53e8 seg10:0x1460 seg10:0x1620 seg10:0x1502] | seg10:0x1460, seg10:0x1620, seg10:0x1502 | seg11:0x530b, seg11:0x53e8 |
| DGROUP:0x8560 | 0x34 × n | struct[] | pod records, one per pool-11 record ([structs.md](structs.md#pool-11-pods)) [BYTES seg13:0x4bb3 seg12:0x6c74] | seg13:0x4ba0, seg12:0x6b1e | seg12:0x6b1e |
| DGROUP:0x8630 | string | char[] | the Strategy-mode command line text that seg7:0x1f38 draws after "Cmd Mode: " ([core.md](core.md#messages-seg30x6ec)) [BYTES seg3:0x3343 seg3:0x3313] | seg3:0x31d0, seg3:0x332e | seg7:0x1f38 |
| DGROUP:0x870c | 4 | DWORD ms | end time of the message line ([core.md](core.md#messages-seg30x6ec)) [BYTES seg3:0x7c6 seg3:0x7fa seg3:0x807 seg3:0x813 seg3:0x81f] | seg3:0x7fa, seg3:0x807, seg3:0x813, seg3:0x81f | — |
| DGROUP:0x8db0 | string | char[] | text of the message line; seg10:0xa332 formats into it and, in mode 2, splits it into the lines DGROUP:0x8768 and DGROUP:0x87b8 [BYTES seg3:0xd2d seg10:0xa831 seg10:0xa843] | seg10:0xa332 | seg3:0x3498 |
| DGROUP:0x8e28 | 4 | DWORD ms | time of the player's last gun shot or weapon-system use [BYTES seg11:0x4ff1 seg11:0xacfa] | seg11:0x4ff1, seg11:0xacfa, seg4:0x587e | seg4:0x57b7, seg4:0x59eb, seg4:0x8642, seg4:0x866d, seg7:0x2190, seg7:0x21d1 [BYTES seg4:0x57b7 seg4:0x587e seg4:0x59eb seg4:0x8642 seg4:0x866d seg7:0x2190 seg7:0x21d1] |
| DGROUP:0x8e60 | 2 | BOOL | pending input present [BYTES seg2:0x783 seg7:0x4e6 seg7:0x513] | seg2:0x783, seg7:0x513 | seg7:0x4e6 |
| DGROUP:0x8e62 | 2 | UINT | pending input msg [BYTES seg2:0x78c seg11:0x6b54] | seg2:0x770, seg2:0x78c, seg11:0x6b54 | seg7:0x105, seg11:0x6ab2 |
| DGROUP:0x8e64 | 2 | WPARAM | pending input wParam [BYTES seg2:0x792] | seg2:0x792 | seg7:0x111 |
| DGROUP:0x8f66 | 2 | BOOL | idle frames allowed; private message 0x9d0 sets it [BYTES seg2:0x4be seg2:0x14a] | seg2:0x4be | seg2:0x14a, seg2:0x12b6 |
| DGROUP:0x8f68 | 2 | BOOL | 0 while the game is paused ([timing.md](timing.md#window-timers)); the input path ignores input then [BYTES seg2:0x9d7 seg2:0xa66 seg2:0x751] | seg2:0x9d7, seg2:0xa66 | seg2:0xc9, seg2:0x751 |
| DGROUP:0x8ff8 | 2 | int | INI key `Complex` (default 0) [BYTES seg6:0x11fb seg6:0x1209] | seg6:0x1209 | seg9:0x3947, seg9:0x5ea7 |
| DGROUP:0x8ffe | 2 | int | icon dissolve step: 3, 2, 1 and 1 for `Detail` 0–3 [BYTES seg6:0xe36 seg6:0xe41 seg6:0xe4f seg6:0xe5d seg6:0xe6d] | seg6:0xe36 … seg6:0xe78 | — |
| DGROUP:0x9000 | 2 | int | 0, 0, 1, 2 and 2 for `Complex` 0–4 ([world.md](world.md#difficulty-inputs)); damage to the player's ship scales by (DGROUP:0x9000 + 6) / 8 ([Scoring](#scoring)) [BYTES seg6:0x1214 seg6:0x122a seg6:0x1238 seg8:0x469d] | seg6:0x1214, seg6:0x122a, seg6:0x1238 | seg8:0x469d, seg9:0x25aa |
| DGROUP:0x90ec | 2 | int | play-area height: the client height DGROUP:0x10 minus the HUD band DGROUP:0xa70 (85) ([shell.md](shell.md#window-geometry)) [BYTES seg2:0xb34 seg2:0xb3d seg2:0xc05 seg2:0x1b29] | seg2:0xb34, seg2:0xb3d, seg2:0xc05, seg2:0x1b29 | 95 instructions, among them seg2:0xce5 and seg15:0x483d |
| DGROUP:0x90f4 | 2 | int | game state ([Game state globals](#game-state-globals)) | see that section | seg7:0xf9, seg11:0x6aef |
| DGROUP:0x90f8 | 2 | BOOL | command line was `DEMO` or `PLAY` [BYTES seg6:0x1c2d seg6:0x1c53] | seg6:0x1c16, seg6:0x1c2d, seg6:0x1c53 | seg2:0x70e |
| DGROUP:0x9100 | 42 | HGLOBAL[21] | pool handles [BYTES seg6:0x1bc2 seg18:0x1e2f] | seg6:0x1bc2, seg18:0x1e2f | seg6:0x1b7c |
| DGROUP:0x9130 | 2 | BOOL | seg18 game mode active ([structs.md](structs.md#pool-roles)). seg18 sets it to 1 and seg10:0xd clears it [BYTES seg18:0x397 seg18:0x8e1 seg10:0xd seg11:0x6abb] | seg18:0x397, seg18:0x8e1, seg10:0xd | seg11:0x6abb, seg7:0x58f |
| DGROUP:0x9144 | 2 | int | pass counter of the frame gate ([timing.md](timing.md#the-frame-gate)) [BYTES seg7:0x1a0 seg7:0x1b3] | seg7:0x1a0, seg7:0x1b3 | seg7:0x1a4 |
| DGROUP:0x9150 | 2 | int | s = 4 − `Size`: one pixel is 1 << s world units ([world.md](world.md#world-coordinates)) [BYTES seg9:0x2408] | seg6:0xb07, seg9:0x2408, seg10:0x9b8d | seg8:0x0, seg10:0x7f7 |
| DGROUP:0x9156 | 2 | int | `Size`, which scales the sprites: a cell of side c is (c << `Size`) >> 1 pixels [BYTES seg10:0xbaa seg10:0xbb1 seg10:0xbb3] | seg6:0xafd, seg9:0x23bd | seg10:0xbaa |
| DGROUP:0x9158 | 2 | int | count of activity rectangles at DGROUP:0x23e0: 1, 2 or 4 ([world.md](world.md#view)) [BYTES seg8:0x551 seg8:0x62a seg8:0x741] | seg8:0x551, seg8:0x62a, seg8:0x741 | seg8:0x4e51, seg11:0x3574 |
| DGROUP:0x9176 | 0x22 × 3 | struct[3] | debris of torn-off wings ([structs.md](structs.md#other-wave-tables)) [BYTES seg8:0x33da seg8:0x33e9] | seg8:0x2cd8 | seg8:0x33da |
| DGROUP:0x91f8 | 2 | int | completed waves; 0 during the set-up of wave 1 ([world.md](world.md#wave-number)) [BYTES seg9:0x5e58 seg9:0x5e60 seg9:0x5fbf] | seg9:0x5fbf, seg9:0xc5c (0) | seg6:0x1258, seg9:0x449c |
| DGROUP:0x91fa, DGROUP:0x91fc, DGROUP:0x91fe | 6 | int[3] | the wave's icon count, min(DGROUP:0x9206, 8), written by the icon walk seg9:0x0; DGROUP:0x91fe sets the rows of the icon column ([world.md](world.md#programs-and-icons)) [BYTES seg9:0x13f seg9:0x142 seg9:0x145]. seg7:0x5a46 recounts DGROUP:0x91fc as the icons still in play: pool-0 records 0 … 7 with `state` 0 and bytes +0x2e and +0x51 at 0. While it is 0 the exit gate flashes OUT ([core.md](core.md#counters-seg70x5ae8-and-seg70x5a46)) [BYTES seg7:0x5a46 seg7:0x5a5f seg12:0x1b42] | seg9:0x13f, seg9:0x142, seg9:0x145, seg7:0x5a5f | seg7:0x6435, seg7:0x6466, seg12:0x1b42 |
| DGROUP:0x9200 | 2 | int | number of the next icon candidate, one per icon index of every file ([world.md](world.md#programs-and-icons)) [BYTES seg9:0x1a seg9:0x241 seg9:0xaf9] | seg9:0x1a, seg9:0x241, seg9:0xaf9 | seg9:0x230, seg9:0xa47 |
| DGROUP:0x9202 | 2 | int | icons placed as objects in this wave [BYTES seg9:0x17 seg9:0xaa1] | seg9:0x17, seg9:0xaa1 | seg9:0x114 |
| DGROUP:0x9204 | 2 | int | icon limit of the walk, 8 [BYTES seg9:0x12 seg9:0x148 seg9:0xbf5] | seg9:0x12, seg9:0x148 | seg9:0x9d1 |
| DGROUP:0x9206 | 2 | int | icon candidates handled, placed or not; the walk stops at 10 [BYTES seg9:0x1d seg9:0x9a0 seg9:0xab7 seg9:0x62] | seg9:0x1d, seg9:0x9a0, seg9:0xab7 | seg9:0x62, seg9:0x21f |
| DGROUP:0x9208 | 2 | int | `Detail`: caps DGROUP:0x932e and picks the drag formula ([world.md](world.md#drift-and-drag)) [BYTES seg6:0xc7b seg9:0x4727 seg9:0x3e6e] | seg6:0xc7b | seg9:0x3e6e, seg9:0x4727 |
| DGROUP:0x9212 | 2 | HINSTANCE | ENG_LANG.DLL, the source of strings and accelerators [BYTES seg2:0xb1 seg3:0x223] [ORACLE probe-0001] | — | seg2:0xb1, seg3:0x223 |
| DGROUP:0x92b0 | 2 | HBITMAP | the ship's body image, ordinal 12's 2H square: the desk's from INI MainShip (seg15:0x2a08), the visit's from the player's design (seg15:0x20ca) ([shell.md](shell.md#game-start-dialogs), item 7) [BYTES seg15:0x2b97 seg15:0x216d] | seg15:0x216d, seg15:0x2b97; freed by seg15:0x14be and seg15:0x4c76 | seg15:0x1a, seg15:0xa1b, seg15:0x2194, seg15:0x2a2f, seg15:0x2c31, seg15:0x2cbe, seg15:0x4cec, seg15:0x4e24, seg15:0x4e89 |
| DGROUP:0x92b2 | 2 | HBITMAP | the ship square with its flame, the source of the launch frames and of the visit's frame 0 [BYTES seg15:0x2bcd seg15:0x2183] | seg15:0x2183, seg15:0x2bcd | seg15:0xb06, seg15:0x1cb5 |
| DGROUP:0x92b4 | 2 | HBITMAP | the visit's ship square without its flame, wings cut as the ship's bytes say, the source of the visit's frame 1 [BYTES seg15:0x2191] | seg15:0x2191, seg15:0x2c9d (0) | seg15:0x2c8d, seg15:0x2c94 |
| DGROUP:0x92b6 | 2 | HDC | the pad DC, holding the pad bitmap DGROUP:0x92a8 that seg15:0x482e draws; freed by seg15:0x152e ([shell.md](shell.md#game-start-dialogs), item 7) [BYTES seg15:0x49d7 seg15:0x4a22] | seg15:0x49b9 (0), seg15:0x49c5 in seg15:0x482e; seg15:0x1579 (0) in seg15:0x152e | seg15:0x0, seg15:0x374c, seg15:0x482e, seg15:0x4a2e, seg15:0x4c76, seg15:0x152e, seg16:0x2144 |
| DGROUP:0x92dc | 2 | int | 200, stored by seg15:0x482e; read by no instruction by its address [BYTES seg15:0x4870] | seg15:0x4870 | none by address |
| DGROUP:0x92da | 2 | int | size s of the launch ship image: 4s is the width of the ship frames ([shell.md](shell.md#game-start-dialogs), item 7) [BYTES seg15:0xa35 seg15:0xa4c] | seg15:0xa35 | seg15:0x15 |
| DGROUP:0x92e4 | 2 | BOOL | keep the player's state: Navigation Enter then requests state 1 and seg9:0xc06 keeps score and wave ([shell.md](shell.md#game-start-dialogs), **Enter**) [BYTES seg6:0x3693 seg9:0xc44] | seg6:0x98c4, seg10:0x9d28, seg16:0x1c3b, seg18:0x5aa, seg18:0x351c (1); seg9:0x698f, seg9:0x6ceb, seg10:0x9da5, seg11:0x7a6c, seg16:0x1c46, seg18:0x724, seg18:0x354a (0) | seg6:0x3693, seg8:0x2d96, seg9:0xc44, seg9:0x6958 |
| DGROUP:0x92e6 | 2 | BOOL | 0 selects the LOADING GRAPHICS branch of seg9:0x22b4, which sets it to 1 ([shell.md](shell.md#game-start-dialogs), **Load screen**) [BYTES seg9:0x22e9 seg9:0x27b3] | seg10:0xa9 (0), seg9:0x27b3 (1) | seg9:0x22e9, seg9:0x2770, seg9:0x27e4 |
| DGROUP:0x92e8 | 2 | BOOL | the wave is Demon's Gate: Navigation Enter sets it for the DEMON'S GATE row ([nav.md](nav.md)) [BYTES seg6:0x30e7 seg9:0x2581] | seg6:0x30e7 (1); seg6:0x3049, seg6:0x30e1, seg6:0x310e (0) | seg9:0x2581, seg8:0x1b84 |
| DGROUP:0x92ea | 2 | int | small-rock count: 14, then DGROUP:0x933a before the rock clamp ([structs.md](structs.md#pool-roles)) [BYTES seg9:0x24cd seg9:0x2540] | seg9:0x24cd, seg9:0x2540 | seg10:0xedb |
| DGROUP:0x92ec | 42 | int[21] | allocated records per pool [BYTES seg7:0x52c7] | seg7:0x52c7, seg14:0xa33, seg9:0x24d3 | seg7:0x52de, seg12:0x276b |
| DGROUP:0x9316 | 2 | int | index of the last ship record (ships − 1) [BYTES seg14:0xa23 seg4:0x8c32] | seg14:0xa23 | seg4:0x8c32, seg7:0xb25 |
| DGROUP:0x9318 | 2 | int | gates in the wave; pool 8 holds two posts and pool 12 two records per gate ([world.md](world.md#gates)) [BYTES seg9:0x4496 seg9:0x2561] | seg9:0x4496, seg9:0x44c3, seg9:0x4680 | seg9:0x255e, seg9:0x117d |
| DGROUP:0x931a | 2 | int | the highest roster index; header +0x34 ([formats.md](formats.md#ship-roster-records-0x7c-bytes)) [BYTES seg6:0x8f88 seg14:0xecd seg10:0x38ce-0x38d6] | seg14:0x0, seg14:0xdd6, seg10:0x3864 (0), seg6:0x930a | seg6:0x8eea, seg9:0x2828 |
| DGROUP:0x931c | 2 | int | the player's roster index; header +0x2c. seg14:0x2300 sets it to 0 when it exceeds DGROUP:0x931a, and the visit's repairs write its record [BYTES seg6:0x8f6c seg14:0x232a-0x2333 seg16:0x1472] | seg6:0x99b6, seg14:0x2300, seg6:0x930a | seg16:0xcfe, seg11:0x72fa |
| DGROUP:0x9320 | 2 | int | wave kind: 6 race track, 7 duel ([Game state globals](#game-state-globals)) | seg9:0x3ac4, seg9:0x3e00 | seg9:0x703c, seg4:0x7071 |
| DGROUP:0x9322 | 2 | int | race course 0–2 (Lazy Loop, SDi Speedway, Grand Prix) ([world.md](world.md#difficulty-inputs)) [BYTES seg9:0x3de6] | seg9:0x3de6 | seg9:0x3e96, seg4:0x240b |
| DGROUP:0x9324 | 2 | int | ship pool index, 14 or 20 [BYTES seg18:0x43b seg18:0x973 seg9:0x22c2] | seg9:0x22c2, seg18:0x43b, seg18:0x973 | seg18:0x208, seg8:0x643b |
| DGROUP:0x9326 | 2 | int | pool index 0, 1 or 15: the first pool the radar draws ([structs.md](structs.md#pool-roles)) [BYTES seg18:0x428 seg18:0x979 seg9:0x2588 seg11:0x35c7] | seg18:0x428, seg18:0x435, seg18:0x979, seg9:0x22cd, seg9:0x2588 | seg11:0x371a, seg11:0x35c7 |
| DGROUP:0x9328 | 2 | int | pool index 5 or 17: the SNOWBALLS and FIREBALLS of seg13:0x1318 ([structs.md](structs.md#pool-roles)) [BYTES seg18:0x441 seg18:0x97f seg13:0x1318] | seg18:0x441, seg18:0x97f | seg8:0x1bb2, seg7:0x4146 |
| DGROUP:0x932a | 2 | int | wave budget: 130, 220, 310, 420 or 2000 for `Complex` 0–4, halved for the first wave and full from wave 11 ([world.md](world.md#difficulty-inputs)) [BYTES seg6:0x120e seg6:0x1240 seg6:0x1258] | seg6:0x120e–seg6:0x1240, seg6:0x1284 | seg6:0x125f, seg9:0x3242 |
| DGROUP:0x932c | 2 | BOOL | seg9:0x2828 sets it to 1 when rand() % 100 falls below 20, 50 or 80 for the classes of DGROUP:0x964a and DGROUP:0x9332, and without rand() for the top class; DGROUP:0x91f8 = 0 clears it. Its use is [OPEN] [BYTES seg9:0x2ae3 seg9:0x2b25 seg9:0x2b35 seg9:0x2b3d seg9:0x2b4d] | seg9:0x2b30, seg9:0x2b35, seg9:0x2ae3 | — |
| DGROUP:0x932e | 2 | int | units of pool 2, 8 records each ([world.md](world.md#difficulty-inputs)) [BYTES seg9:0x4716 seg9:0x24e0] | seg9:0x4716, seg9:0x5e9c | seg9:0x24dc |
| DGROUP:0x9332 | 2 | int | 3, 2, 1 or 0 for DGROUP:0x9432 below 50, 180, 400 or above, set by seg9:0x2828: the class of the next waves. seg9:0x3900 draws one `rand()` only while it is below 2 ([rng.md](rng.md#callers-and-range-mapping)), and the computer ships' choice in seg14 reads it ([world.md](world.md#difficulty-inputs)) [BYTES seg9:0x2a17 seg9:0x2a26 seg9:0x2a36 seg9:0x3900] | seg9:0x2a1e, seg9:0x2a2e, seg9:0x2a3e, seg9:0x2a46 | seg9:0x2a67, seg9:0x2aa6, seg9:0x2f00, seg9:0x3900, seg9:0x6d7f, seg14:0x2544, seg14:0x2caf, seg14:0x2cba, seg14:0x2db8, seg14:0x2dc5, seg14:0x2e07, seg14:0x2e19, seg14:0x3647, seg14:0x3652, seg14:0x38b6, seg14:0x38c1, seg14:0x3903, seg14:0x3915, seg14:0x392f |
| DGROUP:0x9334 | 2 | int | rating level from the DGROUP:0x9bc thresholds [BYTES seg9:0x4882] | seg9:0x4882, seg9:0x38fc | seg9:0x3654 |
| DGROUP:0x9338 | 2 | int | opponents of a race or duel, (code − 1) mod 4; the wave generator also reads it ([world.md](world.md#difficulty-inputs)) [BYTES seg9:0x3e10 seg9:0x3e2b] | seg9:0x3e10, seg9:0x2596 and 36 generator sites | seg9:0x3e2b |
| DGROUP:0x933a | 10 | int[5] | the five object counts of the wave generator: small rocks, big rocks, then pool-8 hazards (DGROUP:0x933e, DGROUP:0x9342) and pool 9 (DGROUP:0x9340) ([world.md](world.md#difficulty-inputs)) [BYTES seg9:0x2513 seg9:0x2520 seg9:0x2530] | the generator seg9:0x3202, seg9:0x2558 | seg9:0x2513 |
| DGROUP:0x9346 | 2 | int | saved as header +0x18f; seg9:0xc06 sets −1 for a new game. Its meaning is [OPEN] [BYTES seg9:0xc44 seg6:0x9071 seg6:0x96b2-0x96b8] | seg9:0xc06, seg6:0x930a | seg6:0x8eea |
| DGROUP:0x934a | 2 | int | pool index 3 or 19: the rock pool, moved by seg7:0x3bae ([structs.md](structs.md#pool-roles)) [BYTES seg18:0x447 seg18:0x985 seg7:0x3bd2] | seg18:0x447, seg18:0x985 | seg7:0x52d4, seg8:0x5eb8, seg7:0x3bd2 |
| DGROUP:0x934e | 2 | int | the wave clock in seconds, lo16(frame# / 36) by seg5:0x2b4; seg16:0xb4e refuses the ambulance below 3 [BYTES seg11:0x1f0e seg11:0x1f17 seg11:0x1f1e seg9:0x6524 seg16:0xb5c] | seg11:0x1f1e, seg9:0x6524 (0) | seg16:0xb5c, seg11:0x1bcc |
| DGROUP:0x939a | 4 | DWORD | the game id, `time()` when the game began; the restore sets a new one before it opens the file and then loads the file's ([formats.md](formats.md#header)) [BYTES seg10:0x805d seg6:0x933f seg6:0x95f7] | seg10:0x82a2, seg6:0x930a | seg6:0x8eea, seg6:0x7042, seg6:0x8776, seg6:0x6576 |
| DGROUP:0x939e, DGROUP:0x93a0 | 4 | int[2] | the start date: (`tm_mon` << 8) OR `tm_mday`, and `tm_year` ([formats.md](formats.md#header)) [BYTES seg6:0x90c1 seg6:0x9610-0x9621 seg6:0x9605-0x9609] | seg10:0x82a2, seg6:0x930a | seg6:0x8eea, seg6:0x6576 |
| DGROUP:0x93a2 | 2 | int | the running game's place in the score list, written only at seg6:0x68eb; a scan that aborts leaves it as it was, 0 in a fresh process ([formats.md](formats.md#score-list)) [BYTES seg6:0x68eb] | seg6:0x68eb | seg6:0x5d58, seg6:0x6576, seg6:0x62b6 |
| DGROUP:0x93a4 | 2 | BOOL | set by the exit-gate routine seg12:0x2288 in two team-7 cases; while it is non-zero the wave end goes to state 8 instead of the debriefing [BYTES seg12:0x2472 seg12:0x24b1 seg9:0x6c5b seg9:0x6c62 seg7:0x687] | seg12:0x2472, seg12:0x24b1 | seg9:0x6bc2, seg7:0x687 |
| DGROUP:0x937a, DGROUP:0x938a, DGROUP:0x9392 | 16 + 8 + 8 | int[8], byte[8], byte[8] | the hit history of the 800 award: lo16(frame# / 10), pool and record of the eight newest player hits ([core.md](core.md#the-vanish-seg70x13b2-and-seg70x158e)) [BYTES seg8:0x8205-0x824c] | seg8:0x81f8 | seg7:0x158e |
| DGROUP:0x93ac, DGROUP:0x94ca, DGROUP:0x967c | 6 | int[3] | saved as header +0x166, +0x168 and +0x18a; seg9:0xc06 zeroes them for a new game. Their meanings are [OPEN] [BYTES seg9:0xc44 seg6:0x9061 seg6:0x9069 seg6:0x9079] | seg9:0xc06, seg6:0x930a | seg6:0x8eea |
| DGROUP:0x9406 | 42 | int[21] | records in use per pool ([structs.md](structs.md#pool-table)) [BYTES seg11:0x376e] | seg7:0x10aa, seg14:0xa2f, seg9:0x24b5 | seg11:0x376e, seg12:0x2761 |
| DGROUP:0x9430 | 2 | int | live entries of the effect table ([structs.md](structs.md#effect-table-45--0x14-bytes-at-dgroup0x1e90)): every creator adds 1 per entry, the effect frame subtracts 1 (floor 0) when an entry ends and the placement seg9:0x6624 zeroes it. The frame loop runs only while it is above 0, and above 10 the generic destroy handler scales its fragment count down [BYTES seg16:0x9c1 seg12:0x2a3c seg8:0x456e seg9:0x670d seg7:0x977 seg12:0x2849 seg12:0x2860] | seg7:0x2f6f, seg8:0x456e, seg8:0x8171, seg8:0x93d0, seg9:0x670d, seg11:0x8b77 … seg11:0x9053 (9 sites), seg12:0xaba, seg12:0x2a3c, seg12:0x2d5f, seg16:0x9c1 | seg7:0x977, seg12:0x2849 |
| DGROUP:0x9434 | 2 | int | the highest effect-table index the frame loop visits (k = 0 … DGROUP:0x9434); the effect frame resets it to the highest busy index when an entry ends, and seg12:0x274a raises it to each fragment's index; seg16:0x8e4 does not write it [BYTES seg7:0x98b seg8:0x4552 seg8:0x4560 seg12:0x28e9] | seg8:0x4552, seg8:0x4560, seg8:0x80ea, seg8:0x813d, seg8:0x9381, seg8:0x93bd, seg9:0x5de5, seg11:0x9018, seg12:0xa7a, seg12:0xab7, seg12:0x28e9, seg12:0x2c43 | seg7:0x98b |
| DGROUP:0x9432 | 2 | int | the violence score: "If you are violent, the game responds with violence, but if you don't try to kill anyone, the game gets less violent." [MANUAL Dealing With Other Ships] A new game sets 100 and a restore the saved header +0x2e. Each gun shot of the player adds 1 and each weapon-system use of a weapon whose W(w)+0x46 = 0 adds 3, unless ship word +0x32 is 9; a gun hit adds a third or a half of its damage, the hit dispatcher a hit's amount, or a half, a quarter or a third of it by the target, for a hit credited to the player, and 200 for the player's kill of a ship ([core.md](core.md#hit-dispatcher-seg80x5df2)); seg4:0x5e8e(x, player, a, 1) adds half its third argument after rewriting 90 to 50, so 25 for a 90, on the path past its early exits; seg13:0x8570 adds 120 for the player when its latched word +0x3e is 14 and 30 otherwise [BYTES seg9:0xc97 seg6:0x966a seg11:0x527a seg11:0x5281 seg11:0xacd1 seg11:0xacd8 seg11:0xacdf seg8:0x7659 seg8:0x7683 seg8:0x6221 seg8:0x62b5 seg8:0x62d8 seg8:0x62ff seg8:0x67e9 seg8:0x7a19 seg4:0x5e9c-0x5ea2 seg4:0x6296-0x629e seg13:0x85e9 seg13:0x8c1e-0x8c33]. **The decay** seg7:0x2128, every 10 s ([DGROUP:0x5b8](#global-table)), with step(q, m) = A −= max(A / q, m), floored at 0: with no other ship (DGROUP:0x9316 = 0) step(50, 2), with no time test; otherwise, when one of ships 0 … DGROUP:0x9316 − 1 has words +0x32 = 14, +0x34 = DGROUP:0x9646 and +0x38 = 1, a ship hunting the player, step(18, 4) and then step(14, 4) when dword DGROUP:0x8e28 + 15000 < now (unsigned); with no hunter step(40, 3) and then step(36, 2) under the same time test [BYTES seg7:0x2135-0x2142 seg7:0x2145-0x2171 seg7:0x2173-0x21b2 seg7:0x21b4-0x21cb seg7:0x21d1-0x21f9]. seg9:0x22b4 scales it by 9/10 when it is below 251 and by 7/10 otherwise at each wave load [BYTES seg9:0x2498 seg9:0x24a0 seg9:0x24a7 seg9:0x24b2]. seg9:0x2828 sets DGROUP:0x9332 from it, the generator's opponent adjustments compare it with 600 and 850 ([world.md](world.md#difficulty-inputs), Target), and above 300 seg14:0x2e55 adds max(A/20, 30) to the weight of an SHP record whose byte +3 is 3, 6 or 1 and seg14:0x3966 max(A/17, 50) to that of a roster record whose byte +0x64 is 3, 6 or 1. The save keeps it as header +0x2e [BYTES seg9:0x2a17 seg9:0x3ce5 seg9:0x3d03 seg14:0x2e38-0x2e6e seg14:0x3949-0x397f seg6:0x8f73]. The rule reproduces core-0003's saved values: 98 in ORACLE03 less 21 solo steps of 2 gives 56 at the wave-1 end, and one wave-2 load to 50 and one step of 3 and 2 gives 45 in ORACLE3X [ORACLE core-0003] | seg4:0x629e, seg6:0x966a, seg7:0x2184, seg7:0x218a, seg7:0x21c5, seg7:0x21cb, seg7:0x21f3, seg7:0x21f9, seg8:0x6221, seg8:0x62b5, seg8:0x62d8, seg8:0x62ff, seg8:0x67e9, seg8:0x7662, seg8:0x768b, seg8:0x7a19, seg9:0xc97, seg9:0x24b2, seg11:0x5281, seg11:0xacdf, seg13:0x8c2c, seg13:0x8c33 | seg6:0x8f70, seg7:0x213c, seg9:0x2498, seg9:0x2a17, seg9:0x3ce5, seg9:0x3d03, seg14:0x2e55, seg14:0x3966 and the writers |
| DGROUP:0x9436, DGROUP:0x9438 | 4 | int[2] | collision damage of party B and party A, written by ISPACE.DLL ordinal 29 through the pointers seg8:0x546c passes; the rock heat DGROUP:0x9454 adds to them. A pool-5 party skips ordinal 29, so they keep the last bounce's or shot impact's values ([core.md](core.md#response-seg80x546c)) [BYTES seg8:0x585b seg8:0x5a90 seg8:0x5867 seg8:0x5a9d seg8:0x579b] | ISPACE.DLL ordinal 29 (calls at seg8:0x57db, seg8:0x5a10, seg8:0x39e5, seg8:0x7417, seg18:0x2ddb), seg8:0x5860, seg8:0x5a96, seg8:0x7abb (halves DGROUP:0x9438), seg18:0x2dee (floor 3) | seg8:0x546c, seg8:0x6f3c |
| DGROUP:0x943a, DGROUP:0x943c | 4 | int[2] | pool and record of the object whose mover is testing collisions; pool 0x1e names a gun shot slot instead [BYTES seg13:0xb75 seg13:0xb7d seg7:0x3f2c seg8:0x4875] | the movers ([core.md](core.md#who-tests-whom-seg80x50aa)) | seg8:0x45e0, seg8:0x4622 |
| DGROUP:0x9442 … DGROUP:0x9450 | 16 | int[8] | collision parties: extents of A and B (DGROUP:0x9442, 0x9444), masses (DGROUP:0x9446, 0x9448), pools (DGROUP:0x944a, 0x944c) and records (DGROUP:0x944e, 0x9450). They keep the last collision's values, which the rock damage handler reads. A gun hit sets the B pool to 30, so the rock handler's in-use guard DGROUP:0x9406 + 2·DGROUP:0x944c reads DGROUP:0x9442, eA, as the count ([core.md](core.md#rocks-seg70x5090)) [BYTES seg8:0x725b seg7:0x50a8 seg8:0x6d79 seg8:0x6db9 seg8:0x6e44 seg8:0x6e9e seg8:0x6f29 seg7:0x509f seg7:0x50e7] | seg8:0x6d79, seg8:0x6e44 | seg8:0x546c, seg7:0x509f |
| DGROUP:0x9454 | 2 | int | the rock's signed heat while the rock mover's collision response runs, 0 otherwise [BYTES seg7:0x400c seg7:0x4011 seg7:0x4049 seg7:0x4068] | seg7:0x4011, seg7:0x4068 | seg8:0x585b, seg8:0x5a90 |
| DGROUP:0x9456 | 2 | int | live smoke records: seg7:0x5b68 and seg7:0x5d6a add 1 for each record they take, seg7:0x5e04 subtracts 1 at a record's end and the placement zeroes it; the frame runs seg7:0x5e04 while it is above 0 ([world.md](world.md#smoke-seg70x5b68-and-seg70x5e04)) [BYTES seg7:0x5c76 seg7:0x5df9 seg7:0x608b seg9:0x670a seg7:0xd0a] | seg7:0x5c76, seg7:0x5df9, seg7:0x608b, seg9:0x670a | seg7:0xd0a, seg18:0x332 |
| DGROUP:0x945c, DGROUP:0x945e | 4 | int[2] | icons captured and destroyed over the run: seg7:0x5ae8 adds 1 to one of them, the restore loads them, and no new game resets them; the save keeps them as header +0x156 and +0x158 [BYTES seg7:0x5b0c seg7:0x5b3d seg6:0x97fe seg6:0x9806 seg6:0x924d seg6:0x9255] | seg7:0x5b0c, seg7:0x5b3d, seg6:0x97fe, seg6:0x9806 | seg6:0x924d |
| DGROUP:0x9460 | 2 | HWND | the Online Help window (class IS_HELP), open during state 7 ([Game state globals](#game-state-globals)) [BYTES seg17:0x57fe seg17:0x600e seg2:0x1ba] | seg17:0x57fe, seg17:0x600e | seg2:0x1ba, seg2:0x1c52 |
| DGROUP:0x94a2 | 2 | int | index of the current directory record at DGROUP:0x69f4 [BYTES seg6:0x3791 seg9:0x4663] | seg6:0x36f3, seg6:0x3791 | seg9:0x4663, seg6:0x3bee |
| DGROUP:0x94a4 | 2 | int | number of directory records [BYTES seg6:0x373c seg6:0x3744] | seg6:0x373c, seg6:0x3744 | seg6:0x36c6, seg6:0x3775 |
| DGROUP:0x94b0 | 2 | BOOL | the entered directory has no record yet: the wave generator draws new counts and the ships all start off-world ([world.md](world.md#ships-at-wave-start)) [BYTES seg6:0x36b6 seg6:0x36ed seg9:0x1128] | seg6:0x36b6, seg6:0x36ed | seg9:0x1128, seg9:0xb49 |
| DGROUP:0x951c | 2 | BOOL | cheat-key mode, toggled by key code 0x2e ([shell.md](shell.md#intro-and-title-screen), **Cheat keys**) [BYTES seg11:0x6ef7 seg11:0x6f00] | seg11:0x6f00, seg6:0x98bb (0), seg10:0x850b (0) | seg11:0x535, seg11:0x6d58, seg11:0x6ef7 |
| DGROUP:0x9522 | 2 | int | 1 when the shield goes up and −1 when it goes down ([core.md](core.md#the-shield-seg110x4b00)); its readers are [OPEN] [BYTES seg11:0x4b74 seg11:0x4bb4] | seg11:0x4b00 | — |
| DGROUP:0x952e | 2 | int | the ship the ambulance serves, −1 when idle ([core.md](core.md#the-ambulance)) [BYTES seg16:0xbf8 seg16:0x8a0 seg9:0x5d52] | seg16:0xbf8, seg16:0x8a0, seg9:0x5cca, seg18:0xff | seg16:0x446, seg11:0x4e1f |
| DGROUP:0x9532 | 2 | HWND | the visit's open dialog: seg16:0xcfe returns at once while it is non-zero, phase 15 holds 1 around the body chooser, and the options dialog stores its hDlg ([core.md](core.md#the-visit-state-6-seg160xcfe)) [BYTES seg16:0xd3b seg16:0x51e7 seg16:0x5b24] | seg16:0x51e7, seg16:0x5b24 (0), seg16:0xcfe | seg16:0xd3b, seg16:0x44fa |
| DGROUP:0x953e … DGROUP:0x957f | bytes | BYTE[] | the visit's purchase flags: DGROUP:0x953e basic repairs, 0x953f wing repair, 0x9540 refuel, 0x9541 double guns, 0x9542 + w weapon w bought, 0x955d + k powerup k, 0x9563 new body, 0x9564 + w weapon w sold, 0x957f unify. The OK button sets them and seg16:0x3784 applies and clears them; seg9:0xc06 clears all but DGROUP:0x957f ([core.md](core.md#purchases-seg160x3784)) [BYTES seg16:0x5b24 seg9:0xcbe-0xd33] | seg16:0x5b24, seg16:0x3b18, seg9:0xc06 | seg16:0x3784, seg16:0xcfe |
| DGROUP:0x9580, DGROUP:0x9584 | 4 | HDC, HBITMAP | the visit's back buffer, W × DGROUP:0x10: made by the descent's first call with the pad in it, drawn into by seg16:0x2144 and seg15:0x454c, blitted whole to the screen and freed at seg16:0x2144's end ([shell.md](shell.md#the-landing-scene)) [BYTES seg15:0x379b-0x3859 seg16:0x271b-0x27d3] | seg15:0x37a4, seg15:0x37b8; seg16:0x27d0, seg16:0x27d3 (0) | seg7:0x1c53, seg15:0x37c2, seg15:0x37d4, seg15:0x37d8, seg15:0x37e4, seg15:0x3836, seg15:0x455b, seg15:0x4562, seg16:0x2152, seg16:0x2159, seg16:0x271b, seg16:0x2787, seg16:0x2799, seg16:0x27ad, seg16:0x27bc, seg16:0x27c5 |
| DGROUP:0x958e | 2 | int | the shop's weapon count: the sign-extended slot count +0xcb at visit phase 10, ±1 per new weapon added or removed; Add>> refuses a new weapon above 10 [BYTES seg16:0x11a9 seg16:0x45f6 seg16:0x46a2] | seg16:0xcfe, seg16:0x44fa, seg16:0x4900 | seg16:0x44fa |
| DGROUP:0x9596 | 2 | int | set to 1 at the ambulance's state 2 → 3; no INSPACE.EXE instruction reads it [BYTES seg16:0x4e1] | seg16:0x4e1 | none |
| DGROUP:0x959e | 2 | int | `Size` at the ambulance's pickup [BYTES seg16:0x492] | seg16:0x492 | seg16:0xcfe |
| DGROUP:0x95a6 | 2 | HGLOBAL | the MASTER.SHP block: seg14:0x3eda returns 1 at once while it is non-zero, else allocates it with GlobalAlloc(0x42, count·0x41) and reads the file into it; seg14:0x4074 unlocks, frees and zeroes it, with the MASTER.BOD block DGROUP:0x95ac, when the far pointer DGROUP:0x6e20 is non-zero. So it is non-zero only inside a seg14:0x3eda/seg14:0x4074 bracket, and 0 in a wave. After a short read seg14:0x3eda frees the block but leaves the handle and DGROUP:0x6e20 set and returns 4 ([ADR 0016](../decisions/0016-undefined-reads.md)) [BYTES seg14:0x3ee9 seg14:0x3f8b-0x3fa1 seg14:0x407e-0x40ca seg14:0x3fd2-0x3fef] | seg14:0x3fa1, seg14:0x40a1 | seg8:0x2d23, seg9:0x2ed6, seg14:0x1380, seg14:0x3ee9, seg14:0x3fb1 |
| DGROUP:0x95b4 | 2 | int | seg14:0x2300's best score, −1000 when no accepted record matches; seg14:0xdd6 adds a roster record only while it exceeds DGROUP:0x95b2 [BYTES seg14:0x2325 seg14:0x317f-0x3190 seg14:0xde5-0xdec] | seg14:0x2300 | seg14:0xdd6 |
| DGROUP:0x95f6 | 2 | int | frame cycle 0–8 of seg13:0x0; thrust, brake and drag run on its even values ([world.md](world.md#ships-seg130x0)) [BYTES seg13:0x18 seg13:0x23 seg13:0x23b] | seg13:0x18, seg13:0x23 | seg13:0x23b, seg11:0x5b6e |
| DGROUP:0x9600 | 2 | BOOL | the frame runs the pool-4 mover seg12:0xae4 while it is non-zero; its only writer stores 1 and nothing clears it ([world.md](world.md#update-loop) step 21) [BYTES seg9:0x6833 seg7:0xd20] | seg9:0x6833 | seg7:0xd20 |
| DGROUP:0x9602 | 2 | int | defenders taken over by the last icon capture [BYTES seg13:0x1ba4 seg13:0x1c1f] | seg13:0x1ba4 | seg13:0x1c1f, seg3:0xe21 |
| DGROUP:0x9604 | 2 | int | resource gain of the last good icon, in hundredths: (icon `damage` × −90) / 210 + 100, 50 when negative ([core.md](core.md#capturing-an-icon)) [BYTES seg13:0x1ae7 seg13:0x1afc seg13:0x1b00] | seg13:0x1af2, seg13:0x1b00 | seg13:0x1b0e, seg3:0xe2d |
| DGROUP:0x9646 | 2 | int | `iMainSmart`, the player's ship record [BYTES seg18:0x203 seg18:0x213] | seg14:0x243, seg11:0xaf29 | seg18:0x203, seg3:0x6fa |
| DGROUP:0x9684 | 2 | int | player's race place ([Game state globals](#game-state-globals)) | seg4:0x8bda | seg4:0x2e98, seg11:0x427f |
| DGROUP:0x968c | 2 | BOOL | a frame is running [BYTES seg2:0x1c72 seg2:0x1d4a seg2:0x766] | seg2:0x1c72, seg2:0x1d4a | seg2:0x766 |
| DGROUP:0x968e | 2 | int | next game state for a pending change: 0, 1 or 4 [BYTES seg2:0x1db6 seg7:0x44d seg6:0x36a0] | seg7:0x44d, seg7:0x69a, seg6:0x36a0 | seg2:0x1db6 |
| DGROUP:0x9e41 | 2 | int | the lift's step, +1, 0 or −1 per frame ([core.md](core.md#the-ambulance)) [BYTES seg16:0x122 seg16:0x130] | seg16:0xa2, seg16:0xcfe, seg16:0x32a0, seg16:0x3306, seg13:0x3748 | seg16:0xa2 |
| DGROUP:0x9e43 | 2 | int | the ambulance's spin phase 0 … 130; the strip cell is it / 10 below 99 ([core.md](core.md#the-ambulance)) [BYTES seg16:0x185 seg16:0x213 seg16:0x22e] | seg16:0xa2, seg16:0xcfe, seg16:0x32a0, seg13:0x3748 | seg16:0xa2 |
| DGROUP:0x9e45 | 2 | int | the ambulance's strip cell, 0 … 13 ([core.md](core.md#the-ambulance)) [BYTES seg16:0x11f seg16:0x195 seg16:0x1c6] | seg16:0xa2, seg9:0x6624, seg16:0xcfe, seg16:0x32a0, seg13:0x3748 | seg16:0xa2, seg13:0x3748 |
| DGROUP:0x9e47 | 2 | int | the lift, 0 … 15 ([core.md](core.md#the-ambulance)) [BYTES seg16:0xfd seg16:0x125 seg16:0x136] | seg16:0xa2, seg16:0xcfe, seg16:0x32a0, seg16:0x3306 | seg16:0xa2, seg8:0xd04 |
| DGROUP:0x9e49 | 2 | int | the ambulance's state 0 … 10; the wave set-up, the briefing and the park routine seg16:0x8a0 set 10, a dispatch 0 ([core.md](core.md#the-ambulance)) [BYTES seg9:0x610b seg9:0x6870 seg9:0x5d4c seg16:0x8a0 seg16:0xbfb] | seg9:0x6624, seg9:0x5cca, seg16:0x8a0, seg16:0xb4e, seg16:0xa2, seg16:0x32a0, seg16:0x3306 | seg16:0xbd7, seg8:0x55b7, seg8:0x6cb3 |
| DGROUP:0x9e4b | 2 | int | the ambulance's distance to its target in cells; a dispatch sets 1400, phase 64 and a lost ship 1200, the wave placement 0 [BYTES seg16:0xbef seg16:0x262 seg16:0x79e seg16:0x1bfb seg9:0x686a seg16:0x600] | seg16:0xb4e, seg16:0xa2, seg16:0xcfe, seg9:0x6624 | seg16:0xa2 |
| DGROUP:0x9e4d | 2 | int | frames without steering after the ambulance hits something: 10 at a hit, −1 per frame [BYTES seg16:0x7cb seg16:0x7d2 seg16:0x7fd] | seg16:0xa2 | seg16:0x649 |
| DGROUP:0x9e4f | 2 | int | the spin step, A's `vx`/8 clamped to 1 … 7 above; its one reader follows the write at seg16:0x1f6 [BYTES seg16:0x1f6 seg16:0x210] | seg16:0x1f6 and dead stores | seg16:0x210 |
| DGROUP:0x9e51 | 2 | int | the ambulance's frame counter 0 … 7: steering on 0 and 1, the pickup turn on 0 and 4 [BYTES seg16:0xc3 seg16:0xce seg16:0x377 seg16:0x650] | seg16:0xa2 | seg16:0xa2 |
| DGROUP:0x9e53 | 2 | int | the distance at the state 2 → 3 step [BYTES seg16:0x506] | seg16:0x506 | seg13:0x3748 |
| DGROUP:0x9e55 | 2 | int | the visit's bill: 600 for a new ship body, the options dialog's Add>> adds and Remove subtracts each price; phase 58 subtracts it, less DGROUP:0x9e5d, from the ship's `credit` and clears it [BYTES seg16:0x139f seg16:0x4758 seg16:0x1a87 seg16:0x1aa0 seg16:0x1aef] [ORACLE core-0003] | seg16:0x139f, seg16:0x44fa, seg16:0x4900, seg16:0x51d6 (0), seg16:0x3b18, seg16:0x1aef | seg16:0x1a87, seg16:0x5820 |
| DGROUP:0x9e57 | 4 | long | shopping credit of the visit's options dialog, the served ship's `credit` at WM_INITDIALOG; Add>> and Remove move each price, and an entry dearer than it is greyed and refused [BYTES seg16:0x51d6 seg16:0x590c seg16:0x44fa] [ORACLE core-0003] | seg16:0x51d6, seg16:0x44fa, seg16:0x4900 | seg16:0x590c, seg16:0x44fa, seg16:0x43f2 |
| DGROUP:0x9e5b | 2 | int | the served ship's `alive` byte as the pickup found it; 0 makes the visit give a new ship body. State 3 of the ship's part seg13:0x3748 sets 0 when the ship is dead, the wave placement 0, the briefing 1, and seg16:0x3784 ends with a dword store of 1 [BYTES seg16:0x43c seg16:0x1365 seg13:0x3772 seg9:0x6867 seg9:0x5d38 seg16:0x3b07] | seg16:0x441, seg13:0x3772, seg9:0x6867, seg9:0x5d38, seg16:0x3784, seg18:0xff | seg16:0x1365, seg16:0x1014 |
| DGROUP:0x9e5d | 2 | int | the Sell total: Add>> in the Sell dialog adds the price; phase 54 shows it and phase 58 subtracts the bill less it; seg16:0x3784's dword write of DGROUP:0x9e5b = 1 zeroes it ([core.md](core.md#the-options-dialog-dialog-0xc2)) [BYTES seg16:0x477a seg16:0x1a55 seg16:0x1a87 seg16:0x3b07] | seg16:0x44fa, seg16:0x4900, seg16:0x541c (0), seg16:0x3784 (0) | seg16:0xcfe, seg16:0x5820 |
| DGROUP:0x9e60 | 4 × n | long[] | per-ship race time; at the finish the code stores the wave frame counter minus the value held there [BYTES seg4:0x2e6e seg4:0x2e7c seg4:0x2e84] | seg4:0x2e84 | seg4:0x2e7c |
| DGROUP:0x9ea0 | 2 | int | race length in laps, compared with `laps` [BYTES seg4:0x261d seg4:0x2dc8] | — | seg4:0x261d |
| DGROUP:0x9ea2 | 2 | int | race flags per lap [BYTES seg4:0x2161 seg4:0x8bc4] | — | seg4:0x2161, seg4:0x8bc4 |
| DGROUP:0xa14d, DGROUP:0xa157 | 20 | BYTE[2][10] | saved as header +0x174 and +0x17e; their meanings are [OPEN] [BYTES seg6:0x9086-0x909c seg6:0x96c9-0x96e6] | seg6:0x930a | seg6:0x8eea |
| DGROUP:0xa161, DGROUP:0xa162 | 2 | BYTE[2] | read from header +0x188 and +0x189, which the writer leaves 0, so a restore zeroes them; seg9:0xc06 sets the dword DGROUP:0xa161 = 0xff for any game [BYTES seg6:0x96f7-0x9704 seg9:0xd33] | seg6:0x930a, seg9:0xc06 | — |
| DGROUP:0xa18f … DGROUP:0xa196 | 8 | BYTE[8] | seg9:0xc06 sets DGROUP:0xa18f … 0xa192 = 1 for any game and DGROUP:0xa193 … 0xa196 = 1 for a new game; the last four are saved as header +0x192 … +0x195. Their meanings are [OPEN] [BYTES seg9:0xcb0 seg9:0xd07 seg6:0x9281-0x92a2] | seg9:0xc06, seg6:0x930a | seg6:0x8eea |
| DGROUP:0xa1cd, DGROUP:0xa1cf | 4 | int[2] | saved as header +0x190/+0x191 and +0x196; seg9:0xc06 sets DGROUP:0xa1cf = 1 and DGROUP:0xa1cd = 0 for a new game. Their meanings are [OPEN] [BYTES seg9:0xc97 seg9:0xc9d seg6:0x925a-0x927c] | seg9:0xc06, seg6:0x930a | seg6:0x8eea |

- A "—" in the writer column means that no writer is listed here, not that none exists. [DESIGN]

### ISPACE.DLL globals of the renderers
| Address | Size | Meaning | Writers | Readers |
|---|---|---|---|---|
| ISPACE.DLL DGROUP:0x1eaa | 2 | 1 while colours 0–9 of the graphic being loaded never brighten along the table, forced to 1 when the scheme byte is not 12; seg7:0x12d8 then takes the scheme as 100, the single ramp ([ispace.md](ispace.md#ordinal-notes), 12) [BYTES ISPACE.DLL seg5:0x2ec2 seg7:0x1541] | ISPACE.DLL seg5:0x2ec2, seg5:0x2fde, seg5:0x310c | ISPACE.DLL seg5:0x3125, seg6:0xd4e, seg7:0x1541 |
| ISPACE.DLL DGROUP:0x1eac | 2 | the same for colours 10–14 [BYTES ISPACE.DLL seg5:0x2fe4 seg7:0x154b] | ISPACE.DLL seg5:0x2fe4, seg5:0x3100, seg5:0x3118 | ISPACE.DLL seg5:0x32d0, seg6:0xd65, seg7:0x154b |
| ISPACE.DLL DGROUP:0x1c70 | 2 | the CPU class 2, 3, 4 or 0 from GetWinFlags ([ispace.md](ispace.md#ordinal-notes), 32, Device texts) [BYTES ISPACE.DLL seg8:0x5f47-0x5f97] | ISPACE.DLL seg8:0x5f5d, seg8:0x5f69, seg8:0x5f84, seg8:0x5f91 | ISPACE.DLL seg8:0x1791, seg8:0x179e, seg8:0x5f9c |
| ISPACE.DLL DGROUP:0x22a8 | 1 | the coprocessor flag: 0xff, then 1 for a 386 with WF_80x87 and 0 for a 486; scene condition 1 [BYTES ISPACE.DLL seg8:0x5f47 seg8:0x169b] | ISPACE.DLL seg8:0x5f47, seg8:0x5f76, seg8:0x5f8a | ISPACE.DLL seg8:0x169b |
| ISPACE.DLL DGROUP:0x22a9 | 1 | 1 when both network reads were non-empty [BYTES ISPACE.DLL seg8:0x5e32 seg8:0x5ebe] | ISPACE.DLL seg8:0x5e32, seg8:0x5e5d, seg8:0x5ebe | none |
| ISPACE.DLL DGROUP:0x22aa | 1 | 1 when wave device 0 exists and lacks WAVECAPS_SYNC; scene condition 3 [BYTES ISPACE.DLL seg8:0x5dd5-0x5e05 seg8:0x17af] | ISPACE.DLL seg8:0x5dd5, seg8:0x5de8, seg8:0x5e05 | ISPACE.DLL seg8:0x17af |
| ISPACE.DLL DGROUP:0x22ab | 1 | 1 when wave device 0 has more than one channel; scene condition 5 [BYTES ISPACE.DLL seg8:0x5dda seg8:0x5e11 seg8:0x17a8] | ISPACE.DLL seg8:0x5dda, seg8:0x5e11 | ISPACE.DLL seg8:0x17a8 |
- The sound and music words of the INSPACE wrappers (DGROUP:0x2384, DGROUP:0x3c88, DGROUP:0x9220, DGROUP:0x9222, DGROUP:0x9292 and the last-play record DGROUP:0x921c–0x9290) are in [sound.md](sound.md#inspace-wrappers). [BYTES seg10:0x7512 seg10:0x752a seg10:0x759a seg10:0x7587 seg10:0x74cd]
