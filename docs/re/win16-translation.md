# Win16 translation

This page maps each Win16 construct that INSPACE.EXE, ISPACE.DLL and the shipdisk FACTORY.EXE use to C++17 Win32 code built with MinGW-w64 for 32-bit and 64-bit Windows. Each topic gives what the original does and where, the trap, and the port's translation. A translation marked **needs an ADR** is a modernisation that waits for a record in `docs/decisions/`. [DESIGN]

Sites and counts come from the `isre asm` listings and their `xrefs.json` (`work/asm/<module>/`). A count of call sites counts call instructions. Addresses without a module name are INSPACE.EXE. Reading traps that are not about translation are in [pitfalls.md](pitfalls.md), the ISPACE.DLL interface is in [ispace.md](ispace.md), and the port's timing design is [ADR 0001](../decisions/0001-timestep.md). [DESIGN]

## Code generation

| Module | Instructions with the 0x66 operand-size prefix | RTL segment | 32-bit arithmetic | x87 instructions |
|---|---|---|---|---|
| INSPACE.EXE | 5,147: 386 code with 32-bit registers | seg5 | 386 instructions and RTL helpers | none |
| ISPACE.DLL | none: 16-bit registers only | seg3 | RTL helpers | none |
| FACTORY.EXE | 439 | seg3 | 386 instructions and RTL helpers | none |

[BYTES seg8:0x12b2] [BYTES seg5:0x110] [BYTES ISPACE.DLL seg3:0x130] [BYTES FACTORY.EXE seg3:0x22e]

### Reading the listings
- The listing prints opcode 0x99 as `cdq` and 0x98 as `cwde` also when the 0x66 prefix is absent. Without the prefix they are CWD (AX → DX:AX) and CBW (AL → AX). Only `66 99` is CDQ. [BYTES seg2:0x3e] [BYTES seg8:0x12b0] [REF Capstone 5.0.7 CS_MODE_16]
- 386 code can push two word arguments with one `push imm32`. `push 0x112f060` at seg6:0x23b8 supplies the message WM_SYSCOMMAND and the wParam SC_CLOSE of the PostMessage at seg6:0x23c2. `push 0x41500c8` at seg6:0x2507 supplies message 0x415 with wParam 200. [BYTES seg6:0x23b8 seg6:0x23c2 seg6:0x2507]
- Dword pushes cross argument boundaries. WndProc hands its frame to DefWindowProc as `push dword ptr [bp + 0xc]` (hwnd and msg), `push dword ptr [bp + 8]` (wParam and HIWORD(lParam)) and `push word ptr [bp + 6]` (LOWORD(lParam)). [BYTES seg2:0x445 seg2:0x449 seg2:0x44d seg2:0x450]
- Arguments of an outer call can be pushed before an inner call runs. The CB_SETCURSEL message at seg6:0x2840 belongs to the SendMessage at seg6:0x285e, and the GetPrivateProfileInt in between supplies its wParam. [BYTES seg6:0x2840 seg6:0x2851 seg6:0x285e]

## Integer semantics

### Types and wrap
- `int` is 16 bits and `long` 32 bits. The port gives every game variable a fixed-width type: `int16_t` or `uint16_t` for a word, `int32_t` or `uint32_t` for a dword, `int8_t` or `uint8_t` for a byte. It does not use `int`, `long` or `char` for game state. [DESIGN]
- 16-bit products wrap. `imul r/m16` leaves the 32-bit product in DX:AX. A following `cwd`, or a store of AX alone, keeps the low 16 bits. A push or store of DX keeps all 32 bits. [timing.md](timing.md#adaptive-wave-threshold) and [globals.md](globals.md#scoring) give the wrapping sites they cover. seg9:0x4180–0x418b keeps DX:AX and then multiplies in 32 bits with `imul ecx`. [BYTES seg7:0xa44] [BYTES seg9:0x4180 seg9:0x4187 seg9:0x4190]
- The port computes in `int32_t` and narrows with `static_cast<int16_t>` where the original drops DX. GCC reduces a narrowing conversion modulo 2^16, so the cast carries the wrap and signed overflow never occurs. [REF GCC 14 manual Integers implementation] [DESIGN]
- `cbw` sign-extends a byte and `sub ah, ah` zero-extends one, so the port picks `int8_t` or `uint8_t` per site: seg4:0x3b73 widens a signed byte, and ISPACE.DLL seg10:0x636d an unsigned one. [BYTES seg4:0x3b73] [BYTES ISPACE.DLL seg10:0x636d]

### Compares
- Signed jumps (`jl`, `jg`, `jle`, `jge`) test signed values, and unsigned jumps (`jb`, `ja`, `jbe`, `jae`) unsigned ones. Examples are the pass-counter test `jl` at seg7:0x17b and the 32-bit frame-gate compare `cmp eax` with `jae` at seg7:0x149. [BYTES seg7:0x17b] [BYTES seg7:0x149]
- In 16-bit code a 32-bit compare is a pair. The jump after the high-word compare decides signedness, and the low word always takes an unsigned jump. ISPACE.DLL seg2:0x885–0x88f compares a signed long (`jg`/`jl`, then `jae`). seg7:0x2b–0x37 compares the deadline DGROUP:0x1e54 with the `GetCurrentTime` sample unsigned (`ja`/`jb`, then `jae`). An unsigned jump on a low word therefore says nothing about the variable's type. [BYTES ISPACE.DLL seg2:0x888 seg2:0x88f] [BYTES seg7:0x2f seg7:0x37]

### Division and shifts
- Every 16-bit division outside the RTL is either `cwd; idiv` (signed: 1,015 in INSPACE.EXE, 191 in ISPACE.DLL, 66 in FACTORY.EXE) or `sub dx, dx; div` (unsigned: 7, 10 and none). Both truncate toward zero, as C++ `/` and `%` do on the promoted operands. Byte divisions truncate the same way: `cbw; idiv` with a byte register (for example `idiv cl` at seg4:0x3310) at 23 sites in INSPACE.EXE and 8 in FACTORY.EXE, and `sub ah, ah; div byte ptr` at 5 sites in ISPACE.DLL (ISPACE.DLL seg5:0x6883). [BYTES seg2:0xcbf] [BYTES seg6:0x9911] [BYTES ISPACE.DLL seg9:0x9d4] [BYTES seg4:0x330f seg4:0x3310] [BYTES ISPACE.DLL seg5:0x6881 seg5:0x6883]
- Division by a power of two compiles to a fix-up that also truncates: `cwd; sub ax, dx; sar ax, 1` halves the screen width at seg2:0x3e–0x41, and `cwd; and dx, 3; add ax, dx; sar ax, 2` divides by 4 at seg3:0x2498. The port writes `/ 2` and `/ 4`. [BYTES seg2:0x41] [BYTES seg3:0x2498]
- A plain `sar` is a C `>>` and rounds toward −∞, for example `sar ax, cl` with CL = the `Size` level DGROUP:0x9156 at seg4:0x1649. The port writes `>>` on the signed type, which GCC shifts arithmetically. [BYTES seg4:0x1649] [REF GCC 14 manual Integers implementation]
- `idiv` faults on a zero divisor and on −32768 / −1, and the 32-bit `idiv` of the 386 code faults on INT32_MIN / −1. The RTL catches the fault: start-up seg5:0x996 installs seg5:0xb5c as the INT 0 handler through DOS function 0x25 (seg5:0x9ce). The handler ends the program with run-time error 3, "integer divide by 0" (DGROUP:0x168a), through FatalAppExit at seg5:0xb95. [BYTES seg5:0x9a0 seg5:0x9ce seg5:0xb5e seg5:0xb95] [BYTES DGROUP:0x168a]
- In C++, −32768 / −1 on promoted `int16_t` operands gives 32768 without a fault, and division by zero is undefined. A faithful port divides through helpers that take the fatal exit in both cases. Whether the port keeps that exit or survives needs an ADR. [DESIGN]

### 32-bit helpers

| Operation | INSPACE.EXE | ISPACE.DLL | FACTORY.EXE | Semantics |
|---|---|---|---|---|
| Multiply | seg5:0x314 (144 callers) | seg3:0xfe (349) | seg3:0x1fc (5) | low 32 bits, the same for signed and unsigned |
| Signed divide | seg5:0x110 (285) | seg3:0x130 (337) | seg3:0x22e (9) | truncates toward zero; INT32_MIN / −1 returns INT32_MIN; a zero divisor faults in `div cx` |
| Signed remainder | seg5:0x1aa (9) | seg3:0x310 (1) | none | takes the dividend's sign |
| Unsigned divide | seg5:0x2b4 (23) | seg3:0x1d6 (7) | seg3:0x19c (3) | |
| Unsigned remainder | seg5:0x24a (no caller) | none | seg3:0x362 (no caller) | |
| Shift left | seg5:0x8c0 (41) | seg3:0x1ca (25) | none | DX:AX by CL |
| Arithmetic shift right | seg5:0x346 (36) | seg3:0x2b0 (3) | none | DX:AX by CL |
| Logical shift right | seg5:0x8cc (1) | seg3:0x2bc (2) | none | DX:AX by CL |

[BYTES seg5:0x314 seg5:0x110 seg5:0x1aa seg5:0x24a seg5:0x2b4 seg5:0x8c0 seg5:0x346 seg5:0x8cc] [BYTES ISPACE.DLL seg3:0xfe seg3:0x130 seg3:0x310 seg3:0x1d6 seg3:0x1ca seg3:0x2b0 seg3:0x2bc] [BYTES FACTORY.EXE seg3:0x1fc seg3:0x22e seg3:0x19c seg3:0x362]

- The multiply, divide and remainder helpers take two dwords on the stack and pop them (`retf 8`). The dividend is the dword pushed last ([globals.md](globals.md#scoring)). [BYTES seg5:0x118 seg5:0x1a7]
- The signed divide negates negative operands, divides unsigned and negates the quotient when exactly one operand was negative. That is why INT32_MIN / −1 comes out as INT32_MIN instead of faulting. [BYTES seg5:0x11d seg5:0x135 seg5:0x199 seg5:0x19c]
- The shift helpers clear CH and loop CL times, so a count of 32 or more shifts every bit out: the result is 0, or −1 for a negative arithmetic shift. A C++ shift by 32 or more is undefined, and x86 `shl eax, cl` masks the count to 5 bits, so the port's shift helpers handle counts of 32 and more explicitly. [BYTES seg5:0x346 seg5:0x348 seg5:0x34e] [DESIGN]
- The port writes each helper as an inline function on `int32_t` or `uint32_t` with these edge cases. The 386 instructions of INSPACE.EXE, for example `imul ecx` and `66 cdq` with `idiv` at seg8:0x1a04–0x1a09, map to plain 32-bit arithmetic and to the faulting divide helper. [BYTES seg8:0x1a04 seg8:0x1a07 seg8:0x1a09] [DESIGN]

## Floating point
- None of the three modules uses floating point. No decoded instruction has an x87 opcode (0xD8–0xDF) or WAIT (0x9B). The segments hold no OSFIXUP relocation records, no module imports WIN87EM, and the only interrupt numbers are 21h, 2Fh and 31h, so no emulator interrupt (34h–3Eh) occurs. [BYTES INSPACE.EXE relocation records] [BYTES ISPACE.DLL relocation records] [BYTES FACTORY.EXE relocation records]
- All code bytes outside the OptLoader stub (seg1) decode, apart from alignment padding and 11 bytes of ISPACE.DLL seg3, so no instruction escapes this check. [BYTES INSPACE.EXE segment table] [BYTES ISPACE.DLL segment table]
- The RTL's message table (DGROUP:0x1664) also holds "floating-point support not loaded" (DGROUP:0x178c). Whether any path of the RTL can still raise that message is [OPEN]. [BYTES DGROUP:0x1664 DGROUP:0x178c]
- Scaled quantities are integer multiply-then-divide through the 32-bit helpers, for example the joystick's × 771 / 1000 + 14 and ordinal 29's integer √(a² + b²) ([ispace.md](ispace.md#ordinal-notes)). The port keeps game logic in integers. [BYTES ISPACE.DLL seg9:0x3ed4 seg2:0x850] [DESIGN]

## Memory

### Global blocks
- Every GlobalAlloc of the three modules (36, 36 and 7 call sites) passes 0x42, GMEM_MOVEABLE | GMEM_ZEROINIT. The application's GlobalReAlloc calls pass 0x42 as well. The RTLs' far-heap code passes 2 (GMEM_MOVEABLE; 0x2002 in ISPACE.DLL) in protected mode and 0x20 (0x2020) otherwise (seg5:0x1e56–0x1e66, ISPACE.DLL seg3:0xb4d–0xb5d, FACTORY.EXE seg3:0xa98–0xaa8). [BYTES seg6:0x1bb8 seg9:0x4174 seg7:0x52f5] [BYTES seg5:0x1e44 seg5:0x1e56 seg5:0x1e5f seg5:0x1e66] [BYTES ISPACE.DLL seg3:0xb4d seg3:0xb56 seg3:0xb5d] [BYTES FACTORY.EXE seg3:0xa98 seg3:0xaa1 seg3:0xaa8] [REF Win16 SDK GlobalAlloc]
- The game locks a block once and keeps the far pointer. It unlocks before GlobalReAlloc and stores the pointer that the next lock returns ([structs.md](structs.md#allocation)). [BYTES seg7:0x52f5]
- The port holds each block in a zero-filled vector. A reallocation invalidates pointers just as GlobalReAlloc does, so the port refreshes its cached pointers, such as the pool table DGROUP:0x1df8, at the points where the original does. [DESIGN]
- `GlobalSize` returns the allocation rounded up. The file writers and readers and the #6600 splitter depend on that size ([formats.md](formats.md#conventions), [formats.md](formats.md#igraphicdll-6600)). The port computes such sizes with a `global_size(n)` function that applies the rounding, not `n`. [DESIGN]
- Resource lengths come from the NE resource table, padded to the module's alignment unit ([resources.md](resources.md#win16-layouts)). SizeofResource is called at seg10:0x3913 and seg17:0x3255, and in ISPACE.DLL at seg5:0x1c69, seg5:0x257d and seg8:0x5bad. The port takes resource lengths from the same table. [BYTES seg10:0x3913 seg17:0x3255] [BYTES ISPACE.DLL seg5:0x1c69 seg5:0x257d seg8:0x5bad] [DESIGN]
- LocalAlloc, LocalReAlloc, LocalFree and LocalSize appear only in the RTLs, one site each (for example seg5:0x1f9d and ISPACE.DLL seg3:0xbf9), for the C near heap. The port uses the C++ allocator. [BYTES seg5:0x1f9d] [BYTES ISPACE.DLL seg3:0xbf9] [DESIGN]
- GlobalCompact(−1) and GlobalCompact(0) at seg6:0x1ab0 and seg6:0x1ab9 follow a failed trial GlobalAlloc in seg6:0x1a68, a memory probe that no instruction calls (FACTORY.EXE has the same pair at seg4:0x1d0c and seg4:0x1d15). LockSegment and UnlockSegment appear only in the RTLs. The port drops them. [BYTES seg6:0x1a68 seg6:0x1a94 seg6:0x1aac seg6:0x1ab0 seg6:0x1ab9] [BYTES FACTORY.EXE seg4:0x1d0c seg4:0x1d15] [REF Win32 SDK GlobalCompact] [DESIGN]

### Huge pointers
- ISPACE.DLL addresses its bitmap buffers through huge pointers. `__AHSHIFT` (KERNEL.113) has 516 references (seg5 97, seg6 225, seg7 194) and `__AHINCR` (KERNEL.114) 72 (seg1 1, seg3 3, seg6 51, seg7 17). INSPACE.EXE uses them only in its chunked file helpers, the writer seg6:0x9e66 and the reader seg14:0x4006. FACTORY.EXE uses them at seg4:0x274f and seg4:0x295a. [BYTES ISPACE.DLL seg6:0x526 seg6:0x244e] [BYTES seg6:0x9ec3 seg14:0x4060] [BYTES FACTORY.EXE seg4:0x274f seg4:0x295a]
- An indexed access adds a 32-bit offset to the pointer and moves the selector by the high word shifted left by `__AHSHIFT` (ISPACE.DLL seg6:0x51b–0x531). A post-increment carries into the selector with `add ax, 1; sbb cx, cx; and cx, __AHINCR; add dx, cx` (ISPACE.DLL seg6:0x2449–0x2452). The 16-bit index is sign-extended with `cwd` first (seg6:0x51a), so a negative index steps back across selectors. [BYTES ISPACE.DLL seg6:0x51a seg6:0x529 seg6:0x52b seg6:0x2449 seg6:0x2452]
- Wine defines `__AHSHIFT` as 3 and `__AHINCR` as 8. [REF Wine 11.0 dlls/krnl386.exe16/krnl386.exe16.spec]
- The port uses a flat `uint8_t*` and a signed `int32_t` offset, so no selector arithmetic survives. The RTL's huge copy and fill (ISPACE.DLL seg3:0x252, seg3:0x2c8) become `memcpy` and `memset`. [BYTES ISPACE.DLL seg3:0x293 seg3:0x29e seg3:0x2f9] [DESIGN]
- The chunked file helpers move at most 0x7fff bytes per `_lread` or `_lwrite` call and stop on a short transfer ([formats.md](formats.md#conventions)). The port transfers in one call and keeps the short-transfer failure. [BYTES seg6:0x9ea4 seg6:0x9ed8 seg14:0x403f] [DESIGN]
- The OptLoader stub of every module references `__AHINCR` and AllocCStoDSAlias ([pitfalls.md](pitfalls.md#packing-and-relocations)). The port has no stub. [BYTES seg1:0x20] [DESIGN]

### Pointers in records
- A far pointer is 4 bytes, offset then selector. Records that hold far code pointers, such as `on_damage`, `radar_draw` and `on_destroy` of the object record ([structs.md](structs.md#object-record-0x70-bytes)), cannot keep their byte layout in a 64-bit build. The port gives such records native pointers or handler indices, and keeps byte layouts only for records read from or written to files ([formats.md](formats.md)). [DESIGN]

## Messages and window procedures

### Procedures

The common set is WM_CREATE (1), WM_MOVE (3), WM_SIZE (5), WM_SETFOCUS (7), WM_PAINT (0xF), WM_CLOSE (0x10) and WM_SYSCOMMAND (0x112), the messages that the window procedures IS_NAV, IS_HELP, IS_WATCH, IS_ICON and IS_SMART decode in varying subsets. A cell that ends in "only" lists every message its procedure decodes. [BYTES seg6:0x1f22 seg17:0x5782 seg13:0x4f4e seg7:0x6184 seg13:0x613a]

| Procedure | Address | Messages it decodes beyond the common set |
|---|---|---|
| WndProc, class `Inner Space` | seg2:0x224 | see [globals.md](globals.md#wndproc-seg20x224-export-ordinal-1) |
| IS_NAV | seg6:0x1f22 | WM_COMMAND, WM_CTLCOLOR, WM_DRAWITEM, WM_MEASUREITEM, WM_GETMINMAXINFO |
| IS_HELP | seg17:0x5782 | WM_COMMAND, WM_CTLCOLOR, WM_DRAWITEM, WM_MEASUREITEM |
| IS_WATCH, IS_ICON, IS_SMART | seg13:0x4f4e, seg7:0x6184, seg13:0x613a | WM_GETMINMAXINFO |
| IS_INFO | seg11:0x22fa | WM_PAINT, WM_LBUTTONDBLCLK only |
| Dialog #194 Ambulance Options, #378 Ship Options | seg16:0x4104, seg16:0x42a2 | WM_INITDIALOG, WM_COMMAND notifications, WM_DRAWITEM, WM_MEASUREITEM |
| Dialog #404 Search | seg17:0x8418 | WM_INITDIALOG, WM_COMMAND with EN_CHANGE |
| Dialog #101, modeless | seg9:0x90fe | WM_INITDIALOG only |
| ISPACE.DLL dialog procedures | ordinals 11, 22, 23, 25–28, 30, 37–40, 42 ([ispace.md](ispace.md#exports-inspace-does-not-request)) | WM_INITDIALOG and WM_COMMAND; WM_HSCROLL in ordinals 11 and 38; private 0xa0e in ordinal 30 |
| ISPACE.DLL LevClass | ordinal 21, ISPACE.DLL seg10:0x309c | WM_PAINT, WM_NCPAINT, WM_COMMAND only |
| ISPACE.DLL NotifyClass | ordinal 31, ISPACE.DLL seg9:0x2ac4 | WM_TIMER, MM_MCINOTIFY, MM_WOM_DONE, private 0x84c only |

[BYTES seg6:0x15e7 seg6:0x1628 seg6:0x164a seg6:0x168b seg6:0x14dc seg6:0x1513 seg6:0x1535 seg6:0x156c seg6:0x158e seg6:0x15c5 seg6:0x16ab seg6:0x16e2] [BYTES seg16:0x1332 seg16:0x133c seg16:0x4204 seg16:0x420e seg17:0x593d seg17:0x5947 seg9:0x6672 seg9:0x667b seg10:0x9a8f seg10:0x9a95] [BYTES seg11:0x230a seg11:0x230f] [BYTES ISPACE.DLL seg10:0x42b9 seg9:0x2ab8]

- No procedure of the table tests for WM_ACTIVATE, WM_ACTIVATEAPP, WM_MENUSELECT or WM_PARENTNOTIFY, so DefWindowProc and DefDlgProc handle them. The game polls focus from its timer instead ([timing.md](timing.md#window-timers)). [BYTES seg2:0x224 seg6:0x1f22 seg17:0x5782] [BYTES ISPACE.DLL seg9:0x15cc seg9:0x2ac4 seg10:0x309c]
- A procedure takes Pascal arguments: hwnd at `[bp + 0xe]`, msg at `[bp + 0xc]`, wParam at `[bp + 0xa]`, LOWORD(lParam) at `[bp + 6]` and HIWORD(lParam) at `[bp + 8]` ([globals.md](globals.md#wndproc-seg20x224-export-ordinal-1)). A dialog procedure returns a 16-bit BOOL, or an HBRUSH for WM_CTLCOLOR. The Win32 procedure returns an INT_PTR. [BYTES seg17:0x8425 seg17:0x8450] [REF Win32 SDK DialogProc]
- Dialog and window procedures go to DialogBox, CreateDialog and RegisterClass as plain far addresses, and no module imports MakeProcInstance. The port passes plain functions. [BYTES seg17:0x5944 seg17:0x5947] [DESIGN]

### Parameter packing

| Message | Win16 | Win32 | Sites that decode it | Port |
|---|---|---|---|---|
| WM_COMMAND | wParam = id; LOWORD(lParam) = control; HIWORD(lParam) = notification | LOWORD(wParam) = id; HIWORD(wParam) = notification, 1 for an accelerator; lParam = control | EN_CHANGE (0x300): seg17:0x8456, ISPACE.DLL seg10:0x733. LBN_DBLCLK (2): seg6:0x230f, seg16:0x4240, seg16:0x4261, seg16:0x4390, seg16:0x43b1. Menu and accelerator ids: seg2:0x13b2 ([globals.md](globals.md#wm_command-handler-seg20x13b2)) | decode with LOWORD and HIWORD of wParam; compare ids with LOWORD(wParam), or accelerator commands miss |
| WM_CTLCOLOR (0x19) | one message; wParam = HDC; LOWORD(lParam) = control; HIWORD(lParam) = CTLCOLOR type | seven messages, WM_CTLCOLORMSGBOX + type (0x132–0x138); lParam = control | IS_NAV and IS_HELP pass it to Ctl3dCtlColorEx through seg2:0x1a5a (callers seg6:0x204e, seg17:0x5913) | handle 0x132–0x138; see [CTL3D](#ctl3d) |
| WM_HSCROLL, WM_VSCROLL | wParam = code; LOWORD(lParam) = position; HIWORD(lParam) = scroll bar | LOWORD(wParam) = code; HIWORD(wParam) = position; lParam = scroll bar | Play Speed bar: ISPACE.DLL seg9:0x1cb6 (scroll bar), seg9:0x1cce (code), seg9:0x1cf1 (position). HUD bar: WndProc seg2:0x6a7 → seg11:0x7798, position at seg11:0x77e0 | the 16-bit position of HIWORD(wParam) covers the 0–1000 range |
| WM_DRAWITEM, WM_MEASUREITEM | lParam → structure with 16-bit fields; DRAWITEMSTRUCT.itemID at +4 | 32-bit fields and pointer-sized handles; itemID at +8 | itemID = −1 tests at seg6:0x1f95, seg16:0x412a, seg16:0x42c8, seg17:0x584a | use the Win32 structure members |
| WM_GETMINMAXINFO | MINMAXINFO of 16-bit POINTs: ptMinTrackSize at +0xc, ptMaxTrackSize at +0x10 | 32-bit POINTs at +0x18 and +0x20 | WndProc seg2:0x3f6, seg2:0x3ff, seg2:0x41e–0x430; IS_WATCH seg13:0x4fef–0x5013; IS_ICON seg7:0x62e1–0x630a; IS_SMART seg13:0x6299–0x62c6. IS_NAV stores (560, 350) as one dword at seg6:0x206d | assign the members |
| MM_MCINOTIFY | wParam = flags; LOWORD(lParam) = device | wParam = flags; lParam = device | NotifyClass reads wParam only (ISPACE.DLL seg9:0x2b18), so the lParam difference has no site in the original | keep the wParam test |
| MM_JOY1MOVE, MM_JOY1BUTTONDOWN, MM_JOY1BUTTONUP | wParam = buttons; LOWORD and HIWORD(lParam) = x and y | the same | WndProc input path ([globals.md](globals.md#input-path)) | keep the handler |
| Private 0x8e2–0xa04 (INSPACE.EXE), 0x84c and 0xa0e (ISPACE.DLL) | WM_USER (0x400) + n | WM_USER is 0x400, and the range stays private | posted with zero or small parameters, for example 0x9d0 at seg2:0x4b6; 0x9c5 carries a dword lParam (seg16:0x6492) | keep the numbers; lParam stays 32-bit |

[BYTES seg17:0x8456 seg6:0x230f seg16:0x4240 seg16:0x4390] [BYTES ISPACE.DLL seg10:0x739] [BYTES seg2:0x1a5a seg6:0x204e seg17:0x5913] [BYTES ISPACE.DLL seg9:0x1cb6 seg9:0x1cce seg9:0x1cf1] [BYTES seg2:0x6a7 seg11:0x77e0] [BYTES seg6:0x1f95 seg16:0x412a seg17:0x584a] [BYTES seg2:0x3f6 seg2:0x41e seg13:0x4fef seg7:0x62e1 seg7:0x630a seg13:0x6299 seg6:0x206d] [BYTES ISPACE.DLL seg9:0x2b18] [BYTES seg2:0x4b6 seg16:0x6492] [REF Wine 11.0 dlls/user.exe16/message.c WINPROC_CallProc16To32A] [REF Win32 SDK WM_COMMAND]

- No module imports joySetCapture (MMSYSTEM.106), so MMSYSTEM never sends the MM_JOY1 messages that WndProc handles. Whether anything else sends them is [OPEN]. [BYTES INSPACE.EXE relocation records]
- INSPACE.EXE posts WM_PAINT (0xF) to its own windows to force redraws, for example to the targeting display DGROUP:0xa7c at seg7:0xaf2. A posted WM_PAINT brings no update region with it. DGROUP:0xa7c holds an IS_INFO window (seg11:0xd53). IS_INFO invalidates its whole client area (seg11:0x232f) before BeginPaint (seg11:0x233c), so its posted WM_PAINT paints everything. Its drawing routine seg11:0x238e takes a GetDC DC (seg11:0x23b2) only when called without one, as from seg13:0xb446. Whether Windows 10 handles such a post as Windows 3.1 does is [OPEN]. [BYTES seg7:0xaf2 seg11:0xd4e seg11:0xd53] [BYTES seg11:0x232f seg11:0x233c seg11:0x2349 seg11:0x23a3 seg11:0x23b2 seg13:0xb446]

### Control messages
- Win16 numbers list box, combo box, edit, button and static messages from WM_USER (0x400) up. Win32 renumbers them below 0x400, for example LB_ADDSTRING 0x180, CB_ADDSTRING 0x143 and CB_SETCURSEL 0x14E. A Win16 number sent to a Win32 control is a private message, which the control ignores. [REF Wine 11.0 include/wine/winuser16.h] [REF Win32 SDK winuser.h]
- One Win16 number means a different message in each class: 0x403 is LB_DELETESTRING, CB_ADDSTRING, EM_SETRECT or BM_SETSTATE. The port names the Win32 constant from the class of the target control in the dialog template ([resources.md](resources.md#dialogs)). [REF Wine 11.0 include/wine/winuser16.h]
- SendMessage has 213 call sites in INSPACE.EXE, 120 in ISPACE.DLL and 162 in FACTORY.EXE. The commonest message words are 0x30 (WM_SETFONT, unchanged in Win32), 0x403, 0x407, 0x40e and 0x40a. Decoded examples: CB_SETCURSEL (0x40e) at seg6:0x2840, LB_SELECTSTRING (0x40d) to list box 409 of the Search dialog at seg17:0x8481, LB_FINDSTRINGEXACT (0x423) at seg6:0x3e21. [BYTES seg6:0x2840 seg6:0x285e] [BYTES seg17:0x8478 seg17:0x8481] [BYTES seg6:0x3e21]
- Wine's 16-bit control procedures list the whole mapping. [REF Wine 11.0 dlls/user.exe16/message.c listbox_proc16]

### Structures passed by pointer
- Win16 RECT and POINT hold 16-bit members, and the Win32 ones hold LONGs. The game lets the API write straight into its globals: GetWindowRect into `ShowLeft`…`ShowBottom` (DGROUP:0x1d20, seg2:0xb7c) and GetCursorPos into DGROUP:0x1d48 (seg2:0x55). The port calls the API with a Win32 structure and narrows each member into the 16-bit global. [BYTES seg2:0xb7c seg2:0x55] [DESIGN]
- GetObject on a bitmap passes 14 bytes, the Win16 BITMAP, at all 29 call sites (for example seg10:0x195b). The Win32 BITMAP is 24 bytes, or 32 in a 64-bit build. [BYTES seg10:0x195b] [REF Win32 SDK BITMAP]
- timeGetDevCaps passes 4 bytes, the Win16 TIMECAPS of two 16-bit words (seg6:0x1346). The Win32 TIMECAPS holds two UINTs. [BYTES seg6:0x1346] [REF Win32 SDK TIMECAPS]

## Dialogs and resources

### Templates
- `<Language>_LANG.DLL` holds the game's dialogs, menus, accelerators and strings in the Win16 layouts of [resources.md](resources.md#win16-layouts). The port reads them from the user's copy at run time and builds Win32 objects from them. [DESIGN]

  | Resource | Win16 use | Port |
  |---|---|---|
  | DIALOG | DialogBox and CreateDialog with the language-DLL handle (DGROUP:0x9212; ISPACE.DLL DGROUP:0x690): seg16:0x133f (#194), seg16:0x4211 (#378), seg17:0x594a (#404), seg9:0x667e and seg10:0x9a95 (#101, modeless) and ISPACE ordinal 10 ([ispace.md](ispace.md#ordinal-notes)) | an in-memory Win32 DLGTEMPLATE (UTF-16 strings, DWORD-aligned items, class bytes 0x80–0x85 written as 0xFFFF and the atom) for DialogBoxIndirectParam |
  | MENU | LoadMenu of 101 at seg6:0x17f5, and of 101 or 102 at seg2:0x16ed; LoadMenu of `PopupMenu` (DGROUP:0xff) at seg2:0x1361 for a popup menu (GetSubMenu, TrackPopupMenu) | CreateMenu and AppendMenu from the parsed items |
  | ACCELERATOR | LoadAccelerators `MainAccel` ([globals.md](globals.md#winmain-seg20x0)) | Win32 ACCEL entries {fVirt & 0x7f, key, cmd} for CreateAcceleratorTable |
  | STRING | LoadString, 246 call sites in INSPACE.EXE | the parsed 16-string blocks |
  | EDIT and LISTBOX borders | Win16 edits and list boxes draw a 1-pixel `WS_BORDER`; the oracle's dialogs show it on every edit and list box | Win32 gives a version-4 dialog's edits and list boxes `WS_EX_CLIENTEDGE`; the port removes it at creation through one CBT hook (`gfx::Win16EditScope`), because the edit fixes its text rectangle at WM_NCCREATE ([pitfalls.md](pitfalls.md)). Nested modal dialogs share the one hook: a second hook subclasses each control twice and the window procedure calls itself until the stack overflows |

  [BYTES seg16:0x133f seg16:0x4211 seg17:0x594a seg9:0x667e seg10:0x9a95 seg6:0x17f5 seg2:0x16ed] [BYTES seg2:0x135e seg2:0x1361 seg2:0x1370 seg2:0x139b DGROUP:0xff] [BYTES ENG_LANG.DLL MENU POPUPMENU] [REF Wine 11.0 dlls/user.exe16/dialog.c DIALOG_ParseTemplate16] [REF Wine 11.0 dlls/user.exe16/user.c LoadMenuIndirect16] [REF Wine 11.0 dlls/user.exe16/user.c LoadAccelerators16]
- The strings are cp1252. Through the ANSI (`A`) API their display depends on the system code page, and converted to UTF-16 for the `W` API it does not. The port follows [ADR 0003](../decisions/0003-text-and-paths.md) decisions 1 and 2. [DESIGN]
- Every ENG_LANG template sets DS_SETFONT: 21 use 8 pt Helv and #101 uses 9 pt Arial. The templates use only the predefined classes BUTTON, STATIC, COMBOBOX, EDIT, SCROLLBAR and LISTBOX, so no custom control class needs registering for them. Four list boxes, in #194 and #378, are LBS_OWNERDRAWFIXED; IS_NAV and IS_HELP create their own owner-drawn list boxes (seg6:0x24f3, seg17:0x6b56). [BYTES ENG_LANG.DLL DIALOG] [BYTES seg6:0x24d1 seg6:0x24f3 seg6:0x2527 seg6:0x254e seg17:0x6b38 seg17:0x6b56 seg17:0x6bab seg17:0x6bc9]
- Dialog units scale with the dialog font's average character size. Helv maps to MS Sans Serif through the FontSubstitutes registry key, so the substitute sets the pixel sizes. Wine's and otvdm's sizes differ ([oracle.md](oracle.md#wine-divergences)), and the Windows 3.1 sizes are [OPEN]. Scaling on high-DPI displays needs an ADR (window and scaling). [REF Wine 11.0 loader/wine.inf.in FontSubstitutes]
- INSPACE.EXE registers the window classes `Inner Space`, IS_WATCH, IS_ICON, IS_SMART, IS_NAV, IS_HELP and IS_INFO (seg6:0x14c7–0x16ef), and ISPACE.DLL registers LevClass and NotifyClass. The port registers the same names. [BYTES seg6:0x14c7 seg6:0x1520 seg6:0x16ef] [BYTES ISPACE.DLL seg10:0x42b9 seg9:0x2ab8] [DESIGN]

### CTL3D
- INSPACE.EXE uses CTL3D when `<Windows directory>\CTL3D.DLL` exists. seg2:0x1994 tests for it with OpenFile(OF_EXIST) at seg2:0x19d3, loads it with LoadLibrary at seg2:0x19e0 (handle DGROUP:0x24) and fetches five entry points by name. [BYTES seg2:0x19bd seg2:0x19d3 seg2:0x19e0 seg2:0x19e5]

  | Entry point | Pointer | Use |
  |---|---|---|
  | Ctl3dRegister | DGROUP:0x1d4c | seg2:0x91, before the message loop |
  | Ctl3dAutoSubclass | DGROUP:0x1d50 | seg2:0x99 |
  | Ctl3dUnregister | DGROUP:0x1d54 | seg2:0x20d, after the message loop |
  | Ctl3dSubclassCtl | DGROUP:0x1d58 | wrapper seg2:0x1a7a, 15 callers in seg6 and seg17 |
  | Ctl3dCtlColorEx | DGROUP:0x1d5c | wrapper seg2:0x1a5a, from WM_CTLCOLOR in IS_NAV and IS_HELP |

  [BYTES seg2:0x19f7 seg2:0x1a0c seg2:0x1a21 seg2:0x1a36 seg2:0x1a4b] [BYTES seg2:0x91 seg2:0x99 seg2:0x20d seg2:0x1a6e seg2:0x1a8e]
- ISPACE.DLL does not reference CTL3D, and its dialogs run in INSPACE's task. Whether Ctl3dAutoSubclass gives them the 3D look on Windows 3.1 is [OPEN]. [BYTES ISPACE.DLL]
- CTL3D.DLL is a 16-bit library, and Win32 dialogs draw their controls in 3D without it. Dropping CTL3D for the native look needs an ADR. [BYTES CTL3D.DLL NE header] [REF Win32 SDK Dialog Box Styles DS_3DLOOK] [DESIGN]

## Time

| Original | Sites | Trap | Port |
|---|---|---|---|
| GetCurrentTime (USER.15) | 98 in INSPACE.EXE, 13 in ISPACE.DLL, 3 in FACTORY.EXE | 32-bit milliseconds that wrap after 2^32 ms (49.7 days). Deadlines compare absolute values unsigned (seg7:0x2b–0x37), so a deadline set shortly before the wrap fires at once or only 49.7 days later | Win32 calls it GetTickCount. ADR 0001 decision 5 makes the port's tick a 32-bit counter; the port keeps `uint32_t` and the same compares. A 64-bit tick needs an ADR |
| timeGetTime (MMSYSTEM.607) | seg7:0xb7, seg15:0xd7a, seg15:0xde6; ISPACE.DLL seg6 (6, ordinal 17); FACTORY.EXE seg2:0x1de3 | on Windows NT its resolution can be 5 ms or coarser unless a process calls timeBeginPeriod | ADR 0001 schedules frames from its own clock. Ordinal 17's use is [OPEN] |
| timeGetDevCaps (MMSYSTEM.604) | seg6:0x1348; FACTORY.EXE seg4:0xa0f | Win16 TIMECAPS is 4 bytes. A minimum period below 3 ms enables the fine timer ([timing.md](timing.md#per-pass-work-in-seg70x0)) | Win32 TIMECAPS; under ADR 0001 the fine timer has no role |
| timeBeginPeriod, timeEndPeriod, timeSetEvent | no module imports them | none | calling timeBeginPeriod would be new behaviour and needs an ADR |
| SetTimer, KillTimer | [timing.md](timing.md#window-timers); ISPACE.DLL seg9:0x46d4; FACTORY.EXE seg4:0x54c, seg4:0x7a3 | Win16 has a system-wide limit on timers, hence the start-up probe seg6:0x1397 | Win32 has no such limit, and SetTimer without a window returns a fresh id, which the probe's KillTimer(…, 1) at seg6:0x13b1 would not stop. The port drops the probe |

[BYTES seg7:0x1f seg7:0x2f seg7:0x37] [BYTES seg7:0xb7 seg15:0xd7a] [BYTES ISPACE.DLL seg6:0x5548] [BYTES seg6:0x1348 seg6:0x1351] [BYTES seg6:0x1397 seg6:0x13b1] [BYTES ISPACE.DLL seg9:0x46d4] [BYTES FACTORY.EXE seg4:0xa0f seg4:0x54c] [REF Win32 SDK GetTickCount] [REF Win32 SDK timeGetTime] [REF Win32 SDK SetTimer] [DESIGN]

## Files

### DOS calls through the RTL
- Each RTL DOS call comes in a pair. It tests bit 0 (WF_PMODE) of the `__WINFLAGS` constant stored at seg5:0x10 and calls DOS3Call in protected mode, `int 21h` otherwise (seg5:0x54–0x64). Windows 3.1 runs only in protected mode, and Wine sets the bit, so the `int 21h` branches never run. [BYTES seg5:0x10 seg5:0x54 seg5:0x5d seg5:0x64] [REF Win16 SDK GetWinFlags] [REF Wine 11.0 dlls/krnl386.exe16/krnl386.exe16.spec]
- INSPACE.EXE has 39 DOS3Call sites, all in seg5. ISPACE.DLL has 16 in seg3 and FACTORY.EXE 13 in seg3. The DOS functions (AH) are 0x30 version, 0x2F and 0x1A DTA, 0x4E and 0x4F find ([pitfalls.md](pitfalls.md#reading-code)), 0x2A and 0x2C date and time, 0x35 and 0x25 interrupt vector (the INT 0 handler above), 0x4C exit, 0x3C, 0x3D and 0x3E create, open and close, 0x3F and 0x40 read and write, 0x42 seek, 0x43 attributes, 0x44 device information, and 0x19 and 0x0E current drive. ISPACE.DLL adds 0x36 free space and 0x57 file time. The port uses the C++ library and Win32 file functions instead. [BYTES seg5:0x3b1 seg5:0xea6 seg5:0x102f seg5:0x161f] [BYTES ISPACE.DLL seg3:0x576 seg3:0x5c1] [BYTES FACTORY.EXE seg3:0x16f] [DESIGN]
- The only direct `int 21h` outside an RTL is at ISPACE.DLL seg10:0x6279; ISPACE.DLL also reaches real-mode `int 21h` through DPMI (seg10:0x461e, see [Win16-only APIs](#win16-only-apis)). [BYTES ISPACE.DLL seg10:0x6279 seg10:0x461e]

### Directory walk and names
- The walk uses the RTL's findfirst and findnext with attribute 0x15 for directories and 0x08 for volume labels. [oracle.md](oracle.md#filesystem-walk) gives the patterns and the order. [ORACLE newgame-0001]
- DOS matches attributes inclusively. Normal files always match, and each search bit adds its kind: directories (0x10), system (0x04) and hidden (0x02) entries. Attribute 0x15 therefore leaves out hidden entries. FindFirstFile returns every entry, so the port filters by the DOS rule and reads volume labels with GetVolumeInformation. [REF RBIL INT 21/AH=4Eh] [DESIGN]
- DOS returns upper-case 8.3 names. FindFirstFile returns long names and an 8.3 alias that can be empty. The game's buffers hold 8.3 names and paths of at most 0x6d bytes (`Path` in [formats.md](formats.md#inspaceini-inner-space)). The port holds paths as [ADR 0003](../decisions/0003-text-and-paths.md) decision 3 says. The walk's 8.3 and long-name rules need an ADR (M3's walk ADR). [REF RBIL INT 21/AH=4Eh] [REF Win32 SDK WIN32_FIND_DATA] [DESIGN]
- The New Game dialog probes drives with the RTL's get and select drive (DOS 0x19 and 0x0E, in ISPACE.DLL seg3:0x43c and seg3:0x466, called from seg10:0x207d onward) under SetErrorMode(SEM_FAILCRITICALERRORS) (seg10:0x2087, seg10:0x55df). The port lists drives with GetLogicalDrives. SetErrorMode keeps its meaning in Win32. [BYTES ISPACE.DLL seg10:0x207d seg10:0x2087 seg10:0x55df seg3:0x451 seg3:0x47f] [ORACLE newgame-0001] [DESIGN]

### File API

| Call | INSPACE.EXE | ISPACE.DLL | FACTORY.EXE | Port |
|---|---|---|---|---|
| OpenFile | 20: OF_READ 11, OF_EXIST (0x4000) 5, OF_CREATE \| OF_READWRITE (0x1002) 2, OF_DELETE (0x200) 2 | 8: OF_READ 4, OF_EXIST 3, computed 1 | 7: OF_READ 2, OF_EXIST 4, 0x1002 1 | the Win32 OpenFile path field holds 128 characters, so the port uses CreateFile, GetFileAttributes and DeleteFile |
| _lread, _lwrite, _llseek | 1, 2, 0 | 9, 1, 5 | 1, 2, 0 | 16-bit counts; see the chunked helpers above. ISPACE.DLL's `_lwrite` (seg10:0x47dc) rewrites resources in place ([formats.md](formats.md#resources-rewritten-in-place)); the port never rewrites resources inside the user's modules ([ADR 0002](../decisions/0002-asset-source.md) decision 5) |
| _lopen, _lcreat, _hread, _hwrite | not imported | not imported | not imported | none |

[BYTES seg2:0x19d3 seg6:0x7c1 seg6:0x8a78] [BYTES ISPACE.DLL seg10:0x46b9 seg10:0x47dc] [BYTES FACTORY.EXE seg4:0x26bf] [REF Win32 SDK OpenFile] [DESIGN]

### Profiles
- [formats.md](formats.md#ini-files) holds the INI keys and their readers. `INSPACE.INI` is named without a path, so the profile functions use the copy in the Windows directory. [REF Win16 SDK GetPrivateProfileString]
- From Windows Vista on, writing to the Windows directory needs elevation. A 32-bit process without a manifest is redirected to the VirtualStore, and a 64-bit or manifested process is refused. Where the port keeps INSPACE.INI needs an ADR. [REF Win32 SDK UAC file virtualization]
- GetPrivateProfileInt returns 16 bits in Win16, and Wine truncates its 32-bit result to INT16. The port stores the result into the same 16-bit type as the original. The limits of Windows 3.x on the parsed value are [OPEN]. [REF Wine 11.0 dlls/krnl386.exe16/file.c GetPrivateProfileInt16] [DESIGN]
- The WIN.INI and SYSTEM.INI keys ([formats.md](formats.md#winini-systemini-and-other-files)) feed ISPACE ordinal 24. On Windows NT the profile functions map some sections of these files to the registry (IniFileMapping), and SYSTEM.INI `[386Enh]` has no effect there. Their port needs an ADR, together with ordinal 24. [REF Win32 SDK GetPrivateProfileString] [DESIGN]
- WinExec: seg6:0x7d1 starts GRMSETUP.EXE when INSPACE.INI is missing ([formats.md](formats.md#files-and-sections)), and the distribution in `original.sha256` contains no GRMSETUP.EXE. ISPACE.DLL seg10:0x71b and seg10:0x5ffa run `NOTEPAD <directory>README.TXT` and `NOTEPAD <directory>ORDER.TXT` with SW_SHOW (strings at ISPACE.DLL DGROUP:0x1e7, DGROUP:0x1f0, DGROUP:0x64a, DGROUP:0x653). Win32 WinExec splits an unquoted path at its first space, so the port starts programs with CreateProcess and a quoted path. Replacing the GRMSETUP.EXE path needs an ADR. [BYTES seg6:0x7c1 seg6:0x7d1] [BYTES ISPACE.DLL seg10:0x71b seg10:0x5ffa DGROUP:0x1e7 DGROUP:0x653] [BYTES original.sha256] [REF Win32 SDK WinExec] [DESIGN]

## Graphics

### Palettes
- ISPACE.DLL owns the palette (ordinals 4 and 14, [ispace.md](ispace.md#ordinal-notes)). It builds a LOGPALETTE with palVersion 0x300 and marks the entries from 10 to 11 below the palette size with peFlags 1 (PC_RESERVED), which AnimatePalette requires. AnimatePalette at ISPACE.DLL seg5:0x441a changes entries 10–245 ([oracle.md](oracle.md#palette-and-colour-depth)). [BYTES ISPACE.DLL seg5:0x3619 seg5:0x38bf seg5:0x38ca seg5:0x38e0 seg5:0x441a] [REF Win16 SDK AnimatePalette]
- INSPACE.EXE selects the palette into every frame DC (233 SelectPalette call sites, [timing.md](timing.md#message-loop-and-idle-step)). It answers WM_QUERYNEWPALETTE and WM_PALETTECHANGED with RealizePalette and UpdateColors ([globals.md](globals.md#wndproc-seg20x224-export-ordinal-1)). The colours of ISPACE.DLL #5500 100 are in [formats.md](formats.md#ispacedll-5500-100). [BYTES seg2:0x59a seg2:0x5a2 seg2:0x1c8f]
- A true-colour display has no hardware palette: GetDeviceCaps reports SIZEPALETTE 0, and AnimatePalette and RealizePalette do not recolour pixels already drawn. Fades and colour cycling that only change the palette then do not show. Reproducing them, for example by drawing into an 8-bit DIB section and converting it when presenting, needs an ADR. [ORACLE depth8-0001] [ORACLE depth8-0002] [REF Win32 SDK AnimatePalette]
- ISPACE.DLL turns #4000 resources into device bitmaps with CreateDIBitmap (seg5:0x215f) and reads bits back with GetDIBits (seg5:0x2295) ([ispace.md](ispace.md#ordinal-notes)). BITMAPINFOHEADER and RGBQUAD have the same layout in Win32. Only the huge pointers to the bits change. [BYTES ISPACE.DLL seg5:0x215f seg5:0x2295] [REF Win32 SDK BITMAPINFOHEADER]

### Device queries

| Site | Index | Use | Port |
|---|---|---|---|
| seg6:0x1716, seg6:0x1723 | HORZRES (8), VERTRES (10) | stored in DGROUP:0x90e6 and DGROUP:0x90e8; no other instruction references those words | query as the original does |
| seg6:0x1730, seg6:0x173d | BITSPIXEL (12), PLANES (14) | DGROUP:0x90ee = the larger of the two (seg6:0x1745–0x174d), the colour depth, with 32 further references; DGROUP:0x90f0 = depth ≥ 8 | a true-colour display reports 32 bits; the depth the port reports belongs to the palette ADR |
| seg6:0x1768 | CAPS1 (94), bit C1_TRANSPARENT | stored in DGROUP:0x90f2; no other instruction references it | Wine returns 0 for CAPS1; the port can leave the query out |
| ISPACE.DLL seg5:0x3644–0x366b | SIZEPALETTE (104), NUMRESERVED (106), BITSPIXEL, PLANES | palette creation, ordinal 4 command 0 | query as the original does; values in [oracle.md](oracle.md#palette-and-colour-depth) |
| FACTORY.EXE seg4:0x997–0x9e9 | the five INSPACE.EXE indices | as in INSPACE.EXE | as in INSPACE.EXE |

[BYTES seg6:0x1716 seg6:0x171b seg6:0x1723 seg6:0x1728 seg6:0x1730 seg6:0x173d seg6:0x1745 seg6:0x174d seg6:0x1768 seg6:0x1776] [BYTES ISPACE.DLL seg5:0x3644 seg5:0x3651 seg5:0x365e seg5:0x366b] [BYTES FACTORY.EXE seg4:0x997 seg4:0x9e9] [REF Wine 11.0 dlls/win32u/driver.c]

### Raster operations

| ROP | INSPACE.EXE | ISPACE.DLL | FACTORY.EXE |
|---|---|---|---|
| SRCCOPY 0xCC0020 | BitBlt 147, StretchBlt 22 | BitBlt 6, StretchBlt 2 | BitBlt 22, StretchBlt 5 |
| SRCAND 0x8800C6 | BitBlt 26 | BitBlt 5, StretchBlt 8 | BitBlt 7 |
| SRCPAINT 0xEE0086 | BitBlt 26 | BitBlt 4, StretchBlt 1 | BitBlt 8 |
| MERGECOPY 0xC000CA | BitBlt 8, StretchBlt 1 | StretchBlt 1 | BitBlt 4 |
| 0xC00746 | BitBlt 2 (seg11:0x208c, seg11:0x2278) | none | none |
| 0xA803A9 | BitBlt 4 | none | none |
| 0x220326 | BitBlt 2 | BitBlt 2, StretchBlt 2 | none |
| MERGEPAINT 0xBB0226 | BitBlt 1 (seg7:0x2463) | none | none |
| BLACKNESS 0x000042 | PatBlt 246 | PatBlt 15 | PatBlt 28 |
| PATCOPY 0xF00021 | PatBlt 43 | PatBlt 38 | none |
| WHITENESS 0xFF0062 | PatBlt 5 | PatBlt 6 | none |
| 0xA000C9 | PatBlt 5 | none | none |
| computed | BitBlt 1, StretchBlt 2 | BitBlt 1, StretchBlt 2 | BitBlt 1, StretchBlt 1 |

[BYTES seg7:0x1a69 seg6:0x48c4 seg6:0x4907 seg11:0x2086 seg11:0x2272 seg15:0x4c5 seg13:0x4503 seg7:0x245b seg2:0xc33 seg6:0x756a seg7:0x434a] [BYTES ISPACE.DLL seg5:0x133e seg5:0x140b seg5:0x14fe seg8:0x633] [BYTES FACTORY.EXE seg2:0x3d19 seg2:0x159a]

- Every code in the table is a ternary raster operation. Win32 BitBlt and StretchBlt accept these codes unchanged. The Win32 PatBlt reference lists only PATCOPY, PATINVERT, DSTINVERT, BLACKNESS and WHITENESS, so whether Win32 PatBlt honours 0xA000C9 (5 sites in INSPACE.EXE) is [OPEN]. [REF Win32 SDK Ternary Raster Operations] [REF Win32 SDK PatBlt]
- 0xC00746 has the index byte 0xC0 of MERGECOPY but not its low word 0x00CA. Wine executes the operation of the index byte. What a Windows 3.x display driver did with the low word is [OPEN]. The port passes 0x00C000CA. [BYTES seg11:0x2086 seg11:0x2272] [REF Wine 11.0 dlls/win32u/dibdrv/bitblt.c] [DESIGN]
- Masked sprites pair SRCAND with SRCPAINT (ordinals 6 and 15 in [ispace.md](ispace.md#ordinal-notes)). A StretchBlt with negative extents mirrors the image (ordinal 36), and Win32 StretchBlt mirrors the same way. [BYTES ISPACE.DLL seg5:0x140b seg10:0x2e1a] [REF Win32 SDK StretchBlt]

### Win16-only GDI calls

| Win16 call | Call sites (INSPACE.EXE, ISPACE.DLL, FACTORY.EXE) | Win32 replacement |
|---|---|---|
| MoveTo, which returns the old position packed in a DWORD | 56, 7, 7 | MoveToEx |
| GetTextExtent, which returns width and height packed | 13, 5, 2 | GetTextExtentPoint32 |
| SetWindowOrg, GetWindowOrg | seg16:0x6541, seg16:0x68bc; seg18:0x17b2 | SetWindowOrgEx, GetWindowOrgEx |
| GetCurrentPosition | seg15:0x7a73, seg15:0x82f4 | GetCurrentPositionEx |

[BYTES seg6:0x4c2a seg6:0x2658 seg16:0x6541 seg16:0x68bc seg18:0x17b2 seg15:0x7a73 seg15:0x82f4] [REF Win32 SDK MoveToEx]

- GDI coordinates are 16-bit in Win16 and 32-bit in Win32, so a coordinate that the original lets wrap inside a GDI call lands elsewhere in the port. [REF Win16 SDK POINT] [REF Win32 SDK Coordinate Spaces and Transformations] Whether any drawing relies on that is [OPEN].

## Sound and input

### Sound
- sndPlaySound: ISPACE.DLL seg9:0x11e8 stops the current sound with (NULL, 0), and seg9:0x1333 plays a sample from memory with flags 7 (SND_ASYNC | SND_NODEFAULT | SND_MEMORY) or 0xF (with SND_LOOP), or with flags 3 (SND_ASYNC | SND_NODEFAULT) for a named sound (seg9:0x1311). Win32 uses the same flag values. With SND_MEMORY the buffer must outlive an asynchronous play. [BYTES ISPACE.DLL seg9:0x11e8 seg9:0x1311 seg9:0x1325 seg9:0x1333] [REF Win32 SDK sndPlaySound]
- WaveMix: ISPACE.DLL loads WAVEMIX.DLL 1.70, a 16-bit library, and resolves eleven functions by name ([ispace.md](ispace.md#what-ispacedll-owns)). Neither port build can load it. A mixer of the port's own over waveOut needs an ADR. [BYTES ISPACE.DLL seg9:0x3677] [BYTES WAVEMIX.DLL]
- MCI music goes through mciSendCommand at ISPACE.DLL seg9:0x381b (MCI_OPEN with MCI_OPEN_TYPE | MCI_OPEN_ELEMENT and the device type `sequencer` at ISPACE.DLL DGROUP:0xae4), seg9:0x3851 (MCI_STATUS), seg9:0x38bb (MCI_SET), seg9:0x3a2f (MCI_CLOSE), seg9:0x3a80 (MCI_PAUSE), seg9:0x3b2f and seg9:0x3b4d (MCI_SEEK) and seg9:0x3bc1 (MCI_PLAY with MCI_NOTIFY and MCI_TO). The notification goes to NotifyClass. [BYTES ISPACE.DLL seg9:0x381b seg9:0x3851 seg9:0x38bb seg9:0x3a2f seg9:0x3a80 seg9:0x3b2f seg9:0x3bc1 DGROUP:0xae4]
- The Win16 MCI parameter blocks hold 16-bit device ids and a DWORD callback whose low word is the window. The port declares the Win32 blocks (MCI_OPEN_PARMSA and the others), whose device ids are UINTs and whose callback is a DWORD_PTR. [REF Win32 SDK MCI_OPEN_PARMS] [DESIGN]
- ISPACE.DLL saves and restores the MIDI and wave volumes on focus changes (ordinal 7 commands 3 and 4 in [ispace.md](ispace.md#ordinal-notes)). Since Windows Vista waveOutSetVolume sets the volume of the application's own audio session, so `ForceVol` no longer reaches the system volume. What midiOutSetVolume reaches on Windows 10 is [OPEN]. The port keeps the calls. [BYTES ISPACE.DLL seg9:0xb18 seg9:0xb8e seg9:0x3e72] [REF Win32 SDK waveOutSetVolume] [DESIGN]

### Joystick
- ISPACE.DLL seg9:0x3ffe reads the game port itself. `in al, dx` from port 0x201 at seg9:0x4010 reads the buttons (high nibble, inverted). `pushf; cli` at seg9:0x401e disables interrupts, `out dx, al` at seg9:0x4058 fires the one-shots, and `in al, dx` at seg9:0x405d loops, counting until the axis bits clear. `sti` at seg9:0x40a7 re-enables interrupts. The positions are loop counts, so they depend on CPU speed, and a count that reaches 3 × 65536 gives 0xFFFF. [BYTES ISPACE.DLL seg9:0x400d seg9:0x4010 seg9:0x401e seg9:0x401f seg9:0x4058 seg9:0x405d seg9:0x406f seg9:0x40a7 seg9:0x40b2]
- Ordinal 41 scales the counts and applies the calibration ([ispace.md](ispace.md#ordinal-notes)). INSPACE polls it every 50 ms while `JoyStick` is set ([timing.md](timing.md#game-time)). [BYTES ISPACE.DLL seg9:0x3eb9]
- In Win32 user mode `in`, `out`, `cli` and `sti` raise a privileged-instruction fault. Wine 11.0 emulates port 0x201 for 16-bit code as all ones, "no joystick", so on the oracle the button nibble reads 0 and the count loop runs to its limit. [REF Intel SDM Vol. 1 I/O privilege level] [REF Wine 11.0 dlls/krnl386.exe16/ioports.c DOSVM_inport]
- joyGetPosEx (winmm) is the faithful source of positions and buttons. Mapping its range (wXmin to wXmax of joyGetDevCaps) onto the counts that ordinal 41 and the `JMinX`…`JRangeY` calibration expect ([formats.md](formats.md#inspaceini-inner-space)) needs an ADR. [DESIGN]

### Keyboard
- GetAsyncKeyState has 11 call sites in INSPACE.EXE: VK_SHIFT and VK_SPACE in the per-pass poll (seg11:0x4dde, seg11:0x4dea), VK_RIGHT at seg11:0x5973, VK_MENU at seg11:0x5987 and seg11:0x5a25, VK_LEFT at seg11:0x5a11, VK_DOWN at seg11:0x5f7c, VK_UP at seg11:0x5fb4, VK_CONTROL at seg11:0x6456, VK_RETURN at seg13:0xcd4 and `Z` at seg13:0xce0. Most sites test bit 15, "down now". seg11:0x5f7c and seg11:0x5fb4 test 0x8001, which adds bit 0, "pressed since the last call". [BYTES seg11:0x4dde seg11:0x4dea seg11:0x5973 seg11:0x5987 seg11:0x5a11 seg11:0x5a23 seg11:0x5a25 seg11:0x5f7c seg11:0x5f81 seg11:0x5fb4 seg11:0x6456 seg13:0xcd4 seg13:0xce0]
- Win32 keeps bit 0 for compatibility only and warns that another process can consume it. The port latches key-down transitions from WM_KEYDOWN to supply bit 0. [ADR 0001](../decisions/0001-timestep.md) asks whether the poll itself latches. [REF Win32 SDK GetAsyncKeyState] [DESIGN]
- GetKeyState for VK_CONTROL and VK_MENU (seg11:0x6cde, seg11:0x6cfd, seg16:0xd46, seg16:0x6937, [timing.md](timing.md#speed-keys)), and GetKeyboardState with SetKeyboardState on resume (seg2:0xa1a, seg2:0xa3e), act on the calling thread's key state in Win32, which matches their use here. [BYTES seg11:0x6cde seg16:0xd46 seg2:0xa1a seg2:0xa3e] [REF Win32 SDK SetKeyboardState]

## Win16-only APIs

| API | Sites | What the original does | Port |
|---|---|---|---|
| GetFreeSystemResources (USER.284) | seg6:0x140b, seg7:0x42 | warns at start-up below 30 % GDI resources; the 5 s check of [timing.md](timing.md#per-pass-work-in-seg70x0) | Win32 has no fixed resource heaps; the port reports 100 % |
| GetWinFlags (KERNEL.132) | ISPACE.DLL seg8:0x5f4c (WF_CPU286, for the system description), seg10:0x444d, seg10:0x5645 and seg10:0x62a0 (WF_WINNT, 0x4000), seg10:0x58eb; FACTORY.EXE seg4:0x971 | with WF_WINNT set, skips the DPMI and MSCDEX code below. Wine sets that bit on an NT host | the port follows the WF_WINNT branches |
| __WINFLAGS (KERNEL.178) | RTLs only: seg5:0x10, ISPACE.DLL seg3:0x10 and seg3:0x17, FACTORY.EXE seg3:0x10 | selects DOS3Call or `int 21h` | dropped |
| GetWindowTask (USER.224) with GetActiveWindow | seg2:0x12bf–0x12d6 (focus watch), seg10:0x7419–0x7430; FACTORY.EXE seg2:0x633, seg2:0x63e | compares the task of the system's active window with its own ([timing.md](timing.md#window-timers)) | Win32 GetActiveWindow only sees the calling thread's windows, so the port compares GetWindowThreadProcessId(GetForegroundWindow()) with its own process |
| GetInstanceData (KERNEL.54) | seg6:0x12d2; FACTORY.EXE seg4:0x6f9 | with a previous instance, copies its main-window handle into DGROUP:0x16, maximises that window and quits, or shows "Another version of game already running" (DGROUP:0x4a0) | Win32 hPrevInstance is always NULL; the port finds a running copy with a named mutex and FindWindow on class `Inner Space` |
| SetMessageQueue (USER.266) | seg6:0x1370; FACTORY.EXE seg4:0x763 | asks for 40 entries and retries down to 8 | Win32 queues grow by themselves; dropped |
| GlobalDOSAlloc, GlobalDOSFree (KERNEL.184, KERNEL.185) and DPMI `int 31h` | ISPACE.DLL seg10:0x457b, seg10:0x566e, seg10:0x5998; seg10:0x45f0, seg10:0x5891, seg10:0x5ad9; `int 31h` AX=0300h at seg10:0x461e (helper seg10:0x4602) | issues real-mode `int 21h` AX=440Dh on a drive through the register block ISPACE.DLL DGROUP:0x1f42, with CX=0866h, get media ID (seg10:0x45a9, seg10:0x5698, seg10:0x59c5), or CX=0846h, set media ID (seg10:0x583e). The code serves ordinal 24 and the resource rewriters of [formats.md](formats.md#resources-rewritten-in-place) | no Win32 equivalent. GetVolumeInformation reads a volume serial number, and no Win32 function sets one. The port of this code needs an ADR |
| `int 21h` AX=440Dh CX=0860h, `int 2Fh` AX=150Bh | ISPACE.DLL seg10:0x6279 (get device parameters), seg10:0x62d5 (MSCDEX drive test, skipped under WF_WINNT) | drive classification, next row | GetDriveType |
| GetDriveType (KERNEL.136) | ISPACE.DLL seg10:0x631b, in seg10:0x62f0 | takes a drive number (0 = A:) and returns 0, 2, 3 or 4, never a CD-ROM code. seg10:0x62f0 refines that result with the two calls above into a code from 0 to 7 for ordinal 24; it returns 0 for a drive number above 25 and for any other GetDriveType result | Win32 GetDriveType takes a root path and reports DRIVE_CDROM (5) and DRIVE_RAMDISK (6) itself. What the codes 1–7 mean is [OPEN] |
| WinHelp (USER.171) | seg2:0x1987; ISPACE.DLL seg9:0x330e, seg9:0x420c; FACTORY.EXE seg2:0x13bd | opens the help file | Windows 10 and 11 ship no WinHlp32; help needs an ADR |
| GetVersion (KERNEL.3) | seg5:0x48 (RTL), seg10:0xd3 | the RTL keeps the version; seg10:0xd3 discards the result | dropped |
| SetWindowWord with GWW_ID (−12) | seg2:0xf75, seg2:0xf8b, seg2:0x16bb, seg6:0xd72 | attaches the menu bar in DGROUP:0x90fa (from LoadMenu at seg6:0x17f5 or seg2:0x16ed) to the popup main window, or detaches it with 0; seg2:0x1701 calls SetMenu for the same purpose | SetMenu |
| SetClassWord with GCW_HICON (−14) | seg6:0x2e8, seg6:0xa5d | sets the `GAME` icon | SetClassLongPtr with GCLP_HICON |
| GetWindowWord with GWW_HINSTANCE (−6) or GWW_ID (−12) | ISPACE.DLL seg9:0x164e and 4 more (GWW_HINSTANCE); FACTORY.EXE seg2:0x72c (GWW_ID) | instance and control id of a window | GetWindowLongPtr with GWLP_HINSTANCE; GetDlgCtrlID |
| InitTask, InitApp, WaitEvent, GetDOSEnvironment, LockSegment, UnlockSegment, LocalInit, GetModuleUsage, FatalExit, AllocCStoDSAlias | RTL start-up and library entry, and the OptLoader stub | run-time support | the C++ run time |
| ExtractIcon (SHELL.34) | index −1 at seg6:0x442c and seg9:0x178 (counts icons); index 0 or a variable at seg6:0x4469, seg9:0x1a5, seg9:0x251 (extracts an icon) | counts icons ([pitfalls.md](pitfalls.md#reading-code)) and extracts them | Win32 ExtractIcon also returns the count for index −1; Wine's 16-bit bug is in [oracle.md](oracle.md#wine-divergences) |

[BYTES seg6:0x1409 seg6:0x140b seg6:0x1413 seg7:0x42] [BYTES ISPACE.DLL seg8:0x5f4c seg8:0x5f59 seg10:0x444d seg10:0x4452 seg10:0x5645 seg10:0x62a0 seg10:0x62ab] [BYTES FACTORY.EXE seg4:0x971] [BYTES seg5:0x10] [BYTES ISPACE.DLL seg3:0x10 seg3:0x17] [BYTES seg2:0x12bf seg2:0x12cb seg2:0x12d6 seg10:0x7419] [BYTES seg6:0x12c4 seg6:0x12d2 seg6:0x12f9 seg6:0x1305] [BYTES seg6:0x1367 seg6:0x1370] [BYTES ISPACE.DLL seg10:0x457b seg10:0x4602 seg10:0x4610 seg10:0x461e seg10:0x45a9 seg10:0x5698 seg10:0x583e seg10:0x59c5 DGROUP:0x1f42] [BYTES ISPACE.DLL seg10:0x6271 seg10:0x6279 seg10:0x62cd seg10:0x62d5 seg10:0x62fd seg10:0x6303 seg10:0x631b seg10:0x6320 seg10:0x6328] [BYTES seg2:0x1987] [BYTES ISPACE.DLL seg9:0x330e seg9:0x420c] [BYTES seg10:0xd3 seg10:0xd8] [BYTES seg2:0xf6f seg2:0xf75 seg2:0xf8b seg6:0x17f5 seg6:0x17fa seg2:0x16f2 seg2:0x1701] [BYTES seg6:0x2d8 seg6:0x2e8] [BYTES ISPACE.DLL seg9:0x164e] [BYTES FACTORY.EXE seg2:0x72c] [BYTES seg6:0x442a seg6:0x442c seg6:0x4467 seg6:0x4469 seg9:0x176 seg9:0x178 seg9:0x1a2 seg9:0x1a5 seg9:0x24f seg9:0x251] [REF Wine 11.0 dlls/krnl386.exe16/kernel.c GetWinFlags16] [REF Win32 SDK GetActiveWindow] [REF Win32 SDK WinMain] [REF RBIL INT 31/AX=0300h] [REF RBIL INT 21/AX=440Dh] [REF RBIL INT 2F/AX=150Bh] [REF Win16 SDK GetDriveType] [REF Win32 SDK GetDriveType] [REF Microsoft KB 917607 WinHlp32] [REF Win32 SDK ExtractIcon] [DESIGN]

## Ship Factory
- FACTORY.EXE is 386 code like INSPACE.EXE ([code generation](#code-generation), [32-bit helpers](#32-bit-helpers)). It uses the constructs above in the same way: CTL3D by name (FACTORY.EXE DGROUP:0x36–0x77, `\CTL3D.DLL` at DGROUP:0xe2), the GetWindowTask focus check, timeGetDevCaps, the five GetDeviceCaps queries, GetInstanceData, SetMessageQueue, GetObject with 14 bytes, huge pointers in its file helpers, GlobalAlloc with 0x42, and 21 ISPACE.DLL ordinals ([ispace.md](ispace.md#exports-inspace-does-not-request)). Each translates as described above. [BYTES FACTORY.EXE DGROUP:0x36 DGROUP:0x77 DGROUP:0xe2] [BYTES FACTORY.EXE seg2:0x633 seg4:0xa0f seg4:0x997 seg4:0x6f9 seg4:0x763 seg4:0x274f] [DESIGN]
