"""relay_digest.py on synthetic trace snippets in Wine's 16-bit relay format."""
from __future__ import annotations

import relay_digest as rd

INT21 = "trace:int21:DOSVM_Int21Handler "
LOADER = [  # INSPACE.EXE's OPTLOADER: segment 1 = 02ef, MYALLOC gives seg2 = 031f, seg3 = 0327
    "Call KERNEL.170: ALLOCCSTODSALIAS(02ef) ret=02ef:0168 ds=030f",
    "Ret  KERNEL.170: ALLOCCSTODSALIAS() retval=00000317 ret=02ef:0168 ds=030f",
    "Call KERNEL.668: MYALLOC(4c58,0000,0000) ret=02ef:0380 ds=02af",
    "Ret  KERNEL.668: MYALLOC() retval=031e0000 ret=02ef:0380 ds=02af",
    "Call KERNEL.668: MYALLOC(4c58,0000,0000) ret=02ef:0380 ds=02af",
    "Ret  KERNEL.668: MYALLOC() retval=03260000 ret=02ef:0380 ds=02af",
    "Call USER.5: INITAPP(03a6) ret=031f:0010 ds=03a7",
    "Ret  USER.5: INITAPP() retval=00000001 ret=031f:0010 ds=03a7",
    "Call KERNEL.91: INITTASK() ret=0327:0020 ds=03a7",
    "Ret  KERNEL.91: INITTASK() retval=00000001 ret=0327:0020 ds=03a7",
    "Call KERNEL.30: WAITEVENT(0000) ret=0327:0030 ds=03a7",
    "Ret  KERNEL.30: WAITEVENT() retval=00000000 ret=0327:0030 ds=03a7",
]


def digest(lines, **kw) -> rd.Digest:
    return rd.digest_lines(lines, **kw)


def section(text: str, title: str) -> str:
    """The body of one '## title' section of a rendered digest."""
    body = text.split(f"## {title}", 1)[1]
    return body.split("\n## ", 1)[0]


# ------------------------------------------------------------------ call pairing

def test_nested_createwindow_each_gets_its_hwnd(relay_log):
    lines = relay_log(
        'Call USER.41: CREATEWINDOW(03a79690 "IS_NAV",03a7b742 "Nav",10cc0000,001e,001e,0230,015e,006c,0000,03a6,00000000) ret=033f:1d4a ds=03a7',
        "CallTo16(func=033f:2000,ds=03a7,007c,0001,0000,03a7b000) ss:sp=03a7:e000",
        "Call USER.33: GETCLIENTRECT(007c,03a7:e010) ret=033f:2010 ds=03a7",
        "Ret  USER.33: GETCLIENTRECT() retval=none ret=033f:2010 ds=03a7",
        'Call USER.41: CREATEWINDOW(03a70100 "button",03a70108 "&Enter",50030000,01ce,000f,005a,001a,007c,0001,03a6,00000000) ret=033f:2474 ds=03a7',
        "Ret  USER.41: CREATEWINDOW() retval=000100a0 ret=033f:2474 ds=03a7",
        "RetFrom16() ss:sp=03a7:e000 retval=00000000",
        "Ret  USER.41: CREATEWINDOW() retval=0001007c ret=033f:1d4a ds=03a7",
    )
    d = digest(lines)
    assert d.windows[0].startswith("   0.0 class \"IS_NAV\"") and d.windows[0].endswith("-> hwnd 007c")
    assert '"button"' in d.windows[1] and d.windows[1].endswith("-> hwnd 00a0")
    assert "x=30 y=30 w=560 h=350 parent=006c" in d.windows[0]
    assert d.stacks["00ec"] == []


def test_dialogbox_gets_its_result_after_nested_calls(relay_log):
    lines = relay_log(
        "Call USER.87: DIALOGBOX(02bf,0000010b #010b,006c,041f0ec2) ret=041f:438b ds=0427",
        "Call USER.91: GETDLGITEM(007c,0065) ret=041f:1600 ds=0427",
        "Ret  USER.91: GETDLGITEM() retval=0000007e ret=041f:1600 ds=0427",
        "Call USER.88: ENDDIALOG(007c,0001) ret=041f:1700 ds=0427",
        "Ret  USER.88: ENDDIALOG() retval=00000001 ret=041f:1700 ds=0427",
        "Ret  USER.87: DIALOGBOX() retval=00000001 ret=041f:438b ds=0427",
    )
    d = digest(lines)
    assert d.dialogs == ["   0.0 DIALOGBOX(inst=02bf, template=#010b, owner=006c) @041f:438b -> 0001"]


def test_file_call_with_findfirst_before_its_ret(relay_log):
    lines = relay_log(
        'Call KERNEL.74: OPENFILE(03a7b6da "C:\\\\windows\\\\INSPACE.INI",03a7:b652,4000) ret=033f:0791 ds=03a7',
        INT21 + 'FINDFIRST mask 0x0008 spec "D:\\\\*.*"',
        "Ret  KERNEL.74: OPENFILE() retval=00000005 ret=033f:0791 ds=03a7",
    )
    d = digest(lines)
    assert d.fileops[0].endswith("OPENFILE(C:\\windows\\INSPACE.INI, 03a7:b652, 4000) @033f:0791 -> 0005")
    assert d.fileops[1]["spec"] == "D:\\*.*"
    text = d.render("t.log", 50)
    assert 'FINDFIRST "D:\\*.*" attr 0x0008' in text


def test_retval_none_does_not_crash(relay_log):
    lines = relay_log(
        'Call KERNEL.128: GETPRIVATEPROFILESTRING(03a7 "Inner Space",03a7 "Path",03a7 "",03a7:b000,0050,03a7 "INSPACE.INI") ret=033f:0100 ds=03a7',
        "Ret  KERNEL.128: GETPRIVATEPROFILESTRING() retval=none ret=033f:0100 ds=03a7",
        'Call USER.41: CREATEWINDOW(03a79690 "X",03a7b742 "Y",80000000,0000,0000,0400,0300,0000,0000,03a6,00000000) ret=033f:18aa ds=03a7',
        "Ret  USER.41: CREATEWINDOW() retval=none ret=033f:18aa ds=03a7",
        "Call GDI.80: GETDEVICECAPS(0042,0008) ret=033f:171b ds=03a7",
        "Ret  GDI.80: GETDEVICECAPS() retval=none ret=033f:171b ds=03a7",
        "Call USER.87: DIALOGBOX(02bf,0000010b #010b,006c,041f0ec2) ret=041f:438b ds=0427",
        "Ret  USER.87: DIALOGBOX() retval=none ret=041f:438b ds=0427",
    )
    d = digest(lines)
    text = d.render("t.log", 50)
    assert "[Inner Space] Path: x1, default \"\", none" in text
    assert d.windows[0].endswith("-> none")
    assert d.devcaps[("GETDEVICECAPS", 8)] == "none"
    assert d.dialogs[0].endswith("-> none")


def test_ret_closes_unreturned_inner_calls_and_stray_rets_are_ignored(relay_log):
    lines = relay_log(
        "Call USER.87: DIALOGBOX(02bf,00000141 #0141,006c,041f0f26) ret=041f:442b ds=0427",
        "Call KERNEL.55: THROW(03a7:e000,0001) ret=041f:5000 ds=0427",  # never returns
        "Ret  USER.999: NEVERCALLED() retval=00000000 ret=041f:6000 ds=0427",
        "Ret  USER.87: DIALOGBOX() retval=00000002 ret=041f:442b ds=0427",
    )
    d = digest(lines)
    assert d.dialogs[0].endswith("-> 0002")
    assert d.stacks["00ec"] == []


def test_pairing_is_per_thread(relay_log):
    a = relay_log("Call USER.87: DIALOGBOX(02bf,0000010b #010b,006c,041f0ec2) ret=041f:438b ds=0427", tid="00ec")
    b = relay_log("Call USER.88: ENDDIALOG(007c,0001) ret=041f:1700 ds=0427",
                  "Ret  USER.88: ENDDIALOG() retval=00000001 ret=041f:1700 ds=0427", tid="00f4", t0=1000.01)
    c = relay_log("Ret  USER.87: DIALOGBOX() retval=00000003 ret=041f:438b ds=0427", tid="00ec", t0=1000.03)
    d = digest(a + b + c)
    assert d.dialogs[0].endswith("-> 0003")


# ------------------------------------------------------------------ maps

def test_selector_map_and_return_addresses(relay_log):
    d = digest(relay_log(*LOADER))
    assert d.modules[0]["name"] == "INSPACE.EXE"
    assert d.modules[0]["sel"] == {"02ef": 1, "031f": 2, "0327": 3}
    assert d.where("031f", "0010") == "INSPACE.EXE seg2:0x0010"
    assert d.where("0400", "0010") == "0400:0010"
    assert "seg1=02ef, seg2=031f, seg3=0327; DGROUP selector (most common ds) 03a7" in d.render("t", 9)


def test_resources_use_instance_names(relay_log):
    lines = relay_log(
        *LOADER,
        'Call KERNEL.95: LOADLIBRARY(03a7b524 "C:\\\\INSPACE\\\\IGRAPHIC.DLL") ret=031f:0345 ds=03a7',
        "Ret  KERNEL.95: LOADLIBRARY() retval=000003bf ret=031f:0345 ds=03a7",
        "Call KERNEL.60: FINDRESOURCE(03bf,0000003c #003c,00000fa0 #0fa0) ret=031f:1c5e ds=03a7",
        "Ret  KERNEL.60: FINDRESOURCE() retval=00000a20 ret=031f:1c5e ds=03a7",
        "Call KERNEL.60: FINDRESOURCE(03bf,0000003c #003c,00000fa0 #0fa0) ret=031f:1c5e ds=03a7",
        "Ret  KERNEL.60: FINDRESOURCE() retval=00000a20 ret=031f:1c5e ds=03a7",
        'Call USER.175: LOADBITMAP(03bf,03a70755 "BW_") ret=031f:0646 ds=03a7',
        "Ret  USER.175: LOADBITMAP() retval=00000000 ret=031f:0646 ds=03a7",
        'Call USER.174: LOADICON(03a6,03a70515 "GAME") ret=031f:1496 ds=03a7',
        "Ret  USER.174: LOADICON() retval=00000a40 ret=031f:1496 ds=03a7",
        "Call USER.173: LOADCURSOR(0000,00007f00 #7f00) ret=031f:14a5 ds=03a7",
        "Ret  USER.173: LOADCURSOR() retval=00000a50 ret=031f:14a5 ds=03a7",
        "Call USER.176: LOADSTRING(02bf,0922,03a7:b742,0028) ret=031f:187f ds=03a7",
        "Ret  USER.176: LOADSTRING() retval=00000005 ret=031f:187f ds=03a7",
    )
    text = digest(lines).render("t", 50)
    res = section(text, "Resources")
    assert "- IGRAPHIC.DLL #4000 (1): 60 x2 @" in res
    assert "- IGRAPHIC.DLL BITMAP (1): BW_ @" in res and "(failed x1)" in res
    assert "- INSPACE.EXE ICON (1): GAME @" in res        # 03a6 = DGROUP 03a7 & ~1
    assert "- (system) CURSOR (1): 32512 @" in res
    assert "- 02bf STRING (1): 2338 @" in res              # no LOADLIBRARY seen for 02bf
    assert "03a6 INSPACE.EXE" in section(text, "Instance handles")
    assert "03bf IGRAPHIC.DLL" in section(text, "Instance handles")


def test_window_labels_follow_window_lifetime(relay_log):
    lines = relay_log(
        'Call USER.41: CREATEWINDOW(03a79690 "IS_NAV",03a7b742 "Nav",10cc0000,001e,001e,0230,015e,006c,0000,03a6,00000000) ret=033f:1d4a ds=03a7',
        "Ret  USER.41: CREATEWINDOW() retval=0001007c ret=033f:1d4a ds=03a7",
        'Call USER.41: CREATEWINDOW(03a70100 "button",03a70108 "&Enter",50030000,01ce,000f,005a,001a,007c,0001,03a6,00000000) ret=033f:2474 ds=03a7',
        "Ret  USER.41: CREATEWINDOW() retval=000100a0 ret=033f:2474 ds=03a7",
        "Call USER.56: MOVEWINDOW(00a0,01bf,0005,005a,001a,0001) ret=033f:20b4 ds=03a7",
        "Ret  USER.56: MOVEWINDOW() retval=00000001 ret=033f:20b4 ds=03a7",
        "Call USER.53: DESTROYWINDOW(007c) ret=033f:2ff1 ds=03a7",
        "Ret  USER.53: DESTROYWINDOW() retval=00000001 ret=033f:2ff1 ds=03a7",
        "Call USER.56: MOVEWINDOW(00a0,ffec,0005,005a,001a,0001) ret=041f:1646 ds=0427",
        "Ret  USER.56: MOVEWINDOW() retval=00000001 ret=041f:1646 ds=0427",
        "Call USER.232: SETWINDOWPOS(006c,ffff,0000,0000,0000,0000,0003) ret=033f:18d8 ds=03a7",
        "Ret  USER.232: SETWINDOWPOS() retval=00000001 ret=033f:18d8 ds=03a7",
    )
    geo = digest(lines).geometry
    assert geo[0].endswith("MoveWindow hwnd 00a0 (button) x=447 y=5 w=90 h=26 @033f:20b4")
    assert "DestroyWindow hwnd 007c (IS_NAV)" in geo[1]
    assert "MoveWindow hwnd 00a0 x=-20 y=5" in geo[2]   # the child died with its parent
    assert "SetWindowPos(after=ffff, flags=0003) hwnd 006c x=0 y=0 w=0 h=0" in geo[3]


# ------------------------------------------------------------------ sections

def test_getdevicecaps_answers(relay_log):
    lines = relay_log(
        "Call GDI.80: GETDEVICECAPS(0042,000c) ret=033f:171b ds=03a7",
        "Ret  GDI.80: GETDEVICECAPS() retval=00000008 ret=033f:171b ds=03a7",
        "Call GDI.80: GETDEVICECAPS(0042,0068) ret=033f:1728 ds=03a7",
        "Ret  GDI.80: GETDEVICECAPS() retval=00000100 ret=033f:1728 ds=03a7",
        "Call USER.179: GETSYSTEMMETRICS(0000) ret=033f:1730 ds=03a7",
        "Ret  USER.179: GETSYSTEMMETRICS() retval=00000400 ret=033f:1730 ds=03a7",
    )
    text = digest(lines).render("t", 50)
    assert "DEVICECAPS[12]=8, DEVICECAPS[104]=256, SYSTEMMETRICS[0]=1024" in text


def test_findfirst_findnext_entries(relay_log):
    lines = relay_log(
        INT21 + "SET DISK TRANSFER AREA ADDRESS 03A7:69F8",
        INT21 + 'FINDFIRST mask 0x0015 spec "D:\\\\*.*"',
        INT21 + "FINDNEXT",
        'trace:file:FindNextFileW returning L"DEEP" (L"")',
        INT21 + "returning: AX=0000 BX=0080 CX=0015 DX=b37a EFL=00000202",
        INT21 + "FINDNEXT",
        'trace:file:FindNextFileW returning L"EMPTY" (L"")',
        INT21 + "returning: AX=0000 BX=0080 CX=0015 DX=b37a EFL=00000202",
        INT21 + "FINDNEXT",
        INT21 + "returning: AX=0012 BX=0080 CX=0015 DX=b37a EFL=00000203",
    )
    d = digest(lines)
    s = d.fileops[0]
    assert (s["spec"], s["mask"], s["dta"], s["calls"], s["found"]) == ("D:\\*.*", "0015", "03A7:69F8", 3, ["DEEP", "EMPTY"])


def test_timers(relay_log):
    lines = relay_log(
        "Call USER.10: SETTIMER(006c,0001,03e8,00000000) ret=033f:0a14 ds=03a7",
        "Ret  USER.10: SETTIMER() retval=00000001 ret=033f:0a14 ds=03a7",
        "Call MMSYSTEM.602: TIMESETEVENT(0014,0001,04270100,00000000,0001) ret=041f:0200 ds=0427",
        "Ret  MMSYSTEM.602: TIMESETEVENT() retval=00000001 ret=041f:0200 ds=0427",
    )
    t = digest(lines).timers
    assert t[0].endswith("SetTimer(hwnd=006c, id=0001, 1000 ms, proc=00000000) @033f:0a14 -> 0001")
    assert "timeSetEvent(20 ms, resolution 1 ms, proc=04270100, periodic) @041f:0200 -> 0001" in t[1]


def test_faults_and_winedbg_block(relay_log):
    lines = relay_log(
        "Call KERNEL.18: GLOBALLOCK(0000) ret=03ff:6f76 ds=03a7",
        "Ret  KERNEL.18: GLOBALLOCK() retval=00000000 ret=03ff:6f76 ds=03a7",
        "!wine: Unhandled page fault on read access to FFFFFFFF at address 00006FBC (thread 00ec), starting debugger...",
        "!oracle: winedbg --auto output follows",
        "!WineDbg attached to pid 00e8",
        "!Unhandled exception: page fault on read access to 0xffffffff in 16-bit code (03ff:6fbc).",
        "!Register dump:",
        "! CS:03ff SS:03a7 DS:03a7 ES:0000 FS:0000 GS:0000",
        "! IP:6fbc SP:e0e0 BP:e0f2 FLAGS:0246(   - --  I  Z- -P- )",
        "!Stack dump:",
        "!0x03a7:0xe0e0:  0000 0000",
        "!Backtrace:",
        "!=>0 0x03ff:0x6fbc (0x03a7:0xe0f2)",
        "!  1 0x03ff:0x6f20 (0x03a7:0xe100)",
        "!0x03ff:0x6fbc: repe movsw",
        "!Modules:",
        "!Module  Address  Debug info  Name (5 modules)",
    )
    d = digest(lines)
    assert d.faults[0].startswith("Unhandled page fault on read access to FFFFFFFF at address 00006FBC")
    assert "GLOBALLOCK(0000) @03ff:6f76" in d.faults[0]
    assert d.winedbg == [
        "Unhandled exception: page fault on read access to 0xffffffff in 16-bit code (03ff:6fbc).",
        "Register dump:",
        " CS:03ff SS:03a7 DS:03a7 ES:0000 FS:0000 GS:0000",
        " IP:6fbc SP:e0e0 BP:e0f2 FLAGS:0246(   - --  I  Z- -P- )",
        "Backtrace:",
        "=>0 0x03ff:0x6fbc (0x03a7:0xe0f2)",
        "  1 0x03ff:0x6f20 (0x03a7:0xe100)",
        "0x03ff:0x6fbc: repe movsw",
    ]
    assert "    winedbg:" in d.render("t", 50)


def test_from_until_window(relay_log):
    lines = relay_log(
        *LOADER,
        "Call GDI.80: GETDEVICECAPS(0042,000c) ret=031f:171b ds=03a7",
        "Ret  GDI.80: GETDEVICECAPS() retval=00000008 ret=031f:171b ds=03a7",
        'Call KERNEL.129: WRITEPRIVATEPROFILESTRING(03a7 "Inner Space",03a7 "NavLeft",03a7 "30",03a7 "INSPACE.INI") ret=031f:3000 ds=03a7',
        "Ret  KERNEL.129: WRITEPRIVATEPROFILESTRING() retval=00000001 ret=031f:3000 ds=03a7",
        "Call USER.10: SETTIMER(006c,0002,0032,00000000) ret=031f:0a14 ds=03a7",
        "Ret  USER.10: SETTIMER() retval=00000002 ret=031f:0a14 ds=03a7",
        "Call USER.87: DIALOGBOX(02bf,00000064 #0064,006c,041f0ec2) ret=031f:438b ds=03a7",
        "Ret  USER.87: DIALOGBOX() retval=00000001 ret=031f:438b ds=03a7",
    )
    d = digest(lines, start="NavLeft", until="DIALOGBOX")
    assert d.devcaps == {}
    assert d.ini_writes and "NavLeft=30" in d.ini_writes[0]
    assert len(d.timers) == 1 and "INSPACE.EXE seg2:0x0a14" in d.timers[0]
    assert d.dialogs == []
    assert "- Events limited to lines from `NavLeft` to `DIALOGBOX`" in d.render("t", 50)


def test_split_args_keeps_quoted_commas():
    assert rd.split_args('03a7 "a,b",0001,"x\\"y,z"') == ['03a7 "a,b"', "0001", '"x\\"y,z"']
    assert rd.split_args("") == []


def test_cli_writes_digest(tmp_path, relay_log):
    log = tmp_path / "trace.log"
    log.write_text("".join(relay_log(*LOADER)))
    out = tmp_path / "digest.md"
    assert rd.main([str(log), "-o", str(out)]) == 0
    text = out.read_text()
    assert text.startswith("# Relay digest: trace.log")
    assert "## Faults\n\n    none" in text
