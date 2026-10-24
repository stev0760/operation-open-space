#!/usr/bin/env python3
"""Summarise a Wine +relay/+int21/+file trace of the game into a small markdown digest.

Usage: relay_digest.py TRACE.log [-o DIGEST.md] [--max-list N] [--from REGEX] [--until REGEX]

Input is a log written with oracle/trace.sh (WINEDEBUG=+relay,+int21,+file,+timestamp
and the 16-bit RelayInclude filter). Sections:
  - relay filter in effect (from the log's own load_list lines)
  - selector -> segN map for OPTLOADER modules (INSPACE.EXE, ISPACE.DLL), built from
    the self-loader's KERNEL.MYALLOC calls: the k-th MYALLOC of a module allocates
    segment k+1 (segment 1 is the loader itself, whose CS calls ALLOCCSTODSALIAS)
  - instance handle -> module (LOADLIBRARY/GETMODULEHANDLE results, and DGROUP & ~1
    for the OPTLOADER modules)
  - per-16-bit-API call counts
  - ordered file and directory operations, with the entries each FINDFIRST/FINDNEXT
    delivered (from kernelbase FindNextFileW "returning" lines)
  - INI reads and writes, timers (window and multimedia), windows, window geometry,
    dialogs, MessageBox texts, resources loaded per module
  - GetDeviceCaps/GetSystemMetrics answers, faults (wine's "Unhandled" line and the
    winedbg register dump and backtrace that oracle/trace.sh appends to the log)
Calls pair with their returns through a per-thread call stack, so nested calls (a
window procedure inside CREATEWINDOW, a dialog procedure inside DIALOGBOX) keep the
outer call open until its own Ret line.
--from/--until limit the event sections to the lines from the first match of
--from up to the first later match of --until. The selector and instance maps
always use the whole log, and times stay relative to the log's first line.
Return addresses are printed as segN:0xOFF where the selector map knows the CS,
otherwise as the raw selector:offset. They are return addresses (after the call).
"""
from __future__ import annotations

import argparse
import collections
import dataclasses
import pathlib
import re
import sys

LINE = re.compile(r"^(?:(?P<ts>\d+\.\d+):)?(?P<tid>[0-9a-f]{4}):(?P<body>.*)$")
CALL = re.compile(r"^Call (?P<mod>[A-Z0-9_]+)\.(?P<ord>\d+): (?P<name>\w+)\((?P<args>.*)\) "
                  r"ret=(?P<cs>[0-9a-f]{4}):(?P<ip>[0-9a-f]{4})(?: ds=(?P<ds>[0-9a-f]{4}))?(?P<regs>.*)$")
RET = re.compile(r"^Ret  (?P<mod>[A-Z0-9_]+)\.(?P<ord>\d+): (?P<name>\w+)\(\) "
                 r"retval=(?P<rv>[0-9a-f]+|none) ret=(?P<cs>[0-9a-f]{4}):(?P<ip>[0-9a-f]{4})(?P<rest>.*)$")
INT21 = re.compile(r"^trace:int21:DOSVM_Int21Handler (?P<msg>.*)$")
FOUND = re.compile(r'^trace:file:FindNextFileW returning L"(?P<long>[^"]*)" \(L"(?P<short>[^"]*)"\)')
LOADLIST = re.compile(r'^trace:relay:load_list L"(?P<key>\w+)" = L"(?P<val>[^"]*)"')
FAULT = re.compile(r"^wine: (Unhandled .*)$")
REGS = re.compile(r"(\w+)=([0-9a-f]+)")
DBG_START = re.compile(r"^Unhandled exception: .*$")
DBG_FRAME = re.compile(r"^(?:=>|\s+)\d+ 0x[0-9a-f]")

FILE_APIS = {"OPENFILE", "_LOPEN", "_LCREAT", "LOADLIBRARY", "LOADMODULE", "WINEXEC",
             "EXTRACTICON", "GETMODULEFILENAME", "GETWINDOWSDIRECTORY", "GETSYSTEMDIRECTORY",
             "GETDRIVETYPE", "GETTEMPFILENAME", "DELETEFILE", "SHELLEXECUTE", "FINDEXECUTABLE"}
INI_READ = {"GETPRIVATEPROFILESTRING", "GETPRIVATEPROFILEINT", "GETPROFILESTRING", "GETPROFILEINT"}
INI_WRITE = {"WRITEPRIVATEPROFILESTRING", "WRITEPROFILESTRING"}
INT21_KEEP = ("OPEN", "CREAT", "FINDFIRST", "FINDNEXT", "failed", "CHANGE CURRENT DIRECTORY",
              "MKDIR", "RMDIR", "UNLINK", "DELETE", "RENAME", "GET CURRENT DIRECTORY",
              "SELECT DEFAULT DRIVE", "GET CURRENT DRIVE", "returning", "GET FILE ATTRIBUTES",
              "SET FILE ATTRIBUTES", "Failed to set current drive", "GET FREE DISK")
# Resource loaders: API name -> (index of the name argument, resource type).
# FINDRESOURCE carries its type as the third argument.
RES_APIS = {"FINDRESOURCE": (1, None), "LOADBITMAP": (1, "BITMAP"), "LOADICON": (1, "ICON"),
            "LOADCURSOR": (1, "CURSOR"), "LOADMENU": (1, "MENU"),
            "LOADACCELERATORS": (1, "ACCELERATOR"), "LOADSTRING": (1, "STRING")}
RT_NAMES = {1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING",
            7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 12: "GROUP_CURSOR",
            14: "GROUP_ICON", 16: "VERSION"}
MM_TIMERS = {"TIMESETEVENT", "TIMEKILLEVENT", "TIMEBEGINPERIOD", "TIMEENDPERIOD"}


@dataclasses.dataclass
class Frame:
    """One open 16-bit call on a thread's stack."""
    ts: str | None
    api: str           # MODULE.ORDINAL
    name: str
    args: list[str]
    cs: str
    ip: str
    active: bool       # inside the --from/--until window
    slot: tuple[list, int] | None = None   # event that takes the return value
    res_key: tuple | None = None            # resource this call loads


def split_args(s: str) -> list[str]:
    """Split a relay argument list on commas outside double-quoted strings."""
    out, cur, q, esc = [], [], False, False
    for ch in s:
        if esc:
            cur.append(ch)
            esc = False
        elif ch == "\\" and q:
            cur.append(ch)
            esc = True
        elif ch == '"':
            cur.append(ch)
            q = not q
        elif ch == "," and not q:
            out.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if cur or out:
        out.append("".join(cur).strip())
    return out


def sval(arg: str) -> str:
    """The string part of 'SEGPTR "text"', the #id of a resource arg, else the raw arg."""
    m = re.search(r'"((?:[^"\\]|\\.)*)"', arg)
    if m:
        return m.group(1).replace("\\\\", "\\")
    if arg.endswith("(null)"):
        return "(null)"
    m = re.search(r"#([0-9a-f]+)", arg)
    return f"#{m.group(1)}" if m else arg


def hexint(arg: str) -> int | None:
    try:
        return int(arg.split()[0], 16)
    except (ValueError, IndexError):
        return None


def rvint(rv: str) -> int | None:
    """A relay retval as an int; None for 'none'."""
    return None if rv == "none" else int(rv, 16)


def rvtext(rv: str) -> str:
    v = rvint(rv)
    return "none" if v is None else f"{v:04x}"


def signed16(v: int | None) -> int | None:
    return v - 0x10000 if v is not None and 0x8000 <= v <= 0xffff else v


def res_id(arg: str) -> str:
    """A resource name argument as text: '"NAME"' -> NAME, '#0064' or a bare word -> 100."""
    s = sval(arg)
    if s.startswith("#"):
        return str(int(s[1:], 16))
    v = hexint(s)
    return str(v & 0xffff) if v is not None and v <= 0xffffffff else s


def res_type(arg: str) -> str:
    s = res_id(arg)
    return RT_NAMES.get(int(s), f"#{s}") if s.isdigit() else s


class Digest:
    def __init__(self, start: str | None = None, until: str | None = None) -> None:
        self.t0: float | None = None
        self.t1: float | None = None
        self.lines = 0
        self.filters: dict[str, str] = {}
        self.counts: collections.Counter[str] = collections.Counter()
        self.stacks: dict[str, list[Frame]] = collections.defaultdict(list)
        # --from/--until window
        self.start_re = re.compile(start) if start else None
        self.until_re = re.compile(until) if until else None
        self.started = self.start_re is None
        self.stopped = False
        self.inside: list[str | None] = [None, None]  # times of the first and last line inside
        # self-loader selector map and instance handles
        self.modules: list[dict] = []
        self.loader_cs: dict[str, dict] = {}
        self.last_loadlib = "INSPACE.EXE"
        self.ds_by_cs: dict[str, collections.Counter[str]] = collections.defaultdict(collections.Counter)
        self.instances: dict[str, str] = {"0000": "(system)"}
        self.hwnds: dict[str, tuple[str, str]] = {}  # live hwnd -> (class, parent)
        # event lists
        self.fileops: list = []
        self.ini_reads: dict[tuple, list] = {}
        self.ini_order: list[tuple] = []
        self.ini_writes: list[str] = []
        self.timers: list[str] = []
        self.windows: list[str] = []
        self.geometry: list[str] = []
        self.dialogs: list[str] = []
        self.msgboxes: list[str] = []
        self.resources: dict[tuple, list] = {}  # (hinst, type, name) -> [count, first t, where, failed]
        self.devcaps: dict[tuple, str] = {}
        self.faults: list[str] = []
        self.winedbg: list[str] = []
        self.dbg_part: str | None = None  # section of a winedbg block being read
        self.recent: collections.deque[str] = collections.deque(maxlen=6)
        # findfirst/findnext pairing: DOS keeps each search's state in its DTA
        self.dta = "default"
        self.by_dta: dict[str, dict] = {}
        self.search: dict | None = None  # search the current int21 call works on
        self.fallthrough = False  # FINDFIRST logs its own FINDNEXT half
        self.last_found: str | None = None
        self.probe: dict | None = None  # SELECT DEFAULT DRIVE / GET CURRENT DRIVE run

    # --- helpers -------------------------------------------------------------
    @property
    def active(self) -> bool:
        return self.started and not self.stopped

    def t(self, ts: str | None) -> str:
        if ts is None or self.t0 is None:
            return "      "
        return f"{float(ts) - self.t0:6.1f}"

    def where(self, cs: str, ip: str) -> str:
        for m in self.modules:
            if cs in m["sel"]:
                return f"{m['name']} seg{m['sel'][cs]}:0x{ip}"
        return f"{cs}:{ip}"

    def dgroup(self, m: dict) -> str | None:
        dss: collections.Counter[str] = collections.Counter()
        for cs in m["sel"]:
            dss.update(self.ds_by_cs.get(cs, {}))
        return dss.most_common(1)[0][0] if dss else None

    def module_of(self, hinst: str) -> str:
        """Module name for an instance or module handle, else the raw handle."""
        h = hinst.split()[0][-4:].lower()
        if h in self.instances:
            return self.instances[h]
        for m in self.modules:
            ds = self.dgroup(m)
            if ds and int(ds, 16) | 1 == int(h, 16) | 1:
                return m["name"]
        return h

    def label(self, hwnd: str) -> str:
        return f"hwnd {hwnd} ({self.hwnds[hwnd][0]})" if hwnd in self.hwnds else f"hwnd {hwnd}"

    def forget_windows(self, gone: set[str]) -> None:
        """Drop destroyed windows and, with them, their child windows."""
        while gone:
            for h in gone:
                self.hwnds.pop(h, None)
            gone = {h for h, (_, parent) in self.hwnds.items() if parent in gone}

    def complete(self, frame: Frame, text: str) -> None:
        """Append a return value to the event recorded for this call."""
        if frame.slot is not None:
            lst, i = frame.slot
            lst[i] += text

    def push_event(self, frame: Frame, lst: list, item) -> None:
        lst.append(item)
        frame.slot = (lst, len(lst) - 1)

    def pop_frame(self, tid: str, api: str, cs: str, ip: str) -> Frame | None:
        """Pop the newest open call that this Ret closes, and any calls above it
        (calls that never returned, e.g. a fault or a Throw inside them)."""
        st = self.stacks.get(tid)
        if not st:
            return None
        for i in range(len(st) - 1, -1, -1):
            f = st[i]
            if f.api == api and f.cs == cs and f.ip == ip:
                del st[i:]
                return f
        return None

    # --- line handlers -------------------------------------------------------
    def feed(self, raw: str) -> None:
        self.lines += 1
        line = raw.rstrip("\r\n")
        if not self.started and self.start_re.search(line):
            self.started = True
        elif self.active and self.until_re is not None and self.until_re.search(line):
            self.stopped = True
        if self.on_winedbg(line):
            return
        mf = FAULT.match(line)
        if mf:
            self.faults.append(mf.group(1) + "; preceding calls: " + " | ".join(self.recent))
            return
        m = LINE.match(line)
        if not m:
            return
        ts, tid, body = m.group("ts"), m.group("tid"), m.group("body")
        if ts:
            f = float(ts)
            self.t0 = f if self.t0 is None else self.t0
            self.t1 = f
            if self.active:
                self.inside[0] = self.inside[0] or ts
                self.inside[1] = ts
        if body.startswith("Call "):
            mc = CALL.match(body)
            if mc:
                self.on_call(ts, tid, mc)
            return
        if body.startswith("Ret  "):
            mr = RET.match(body)
            if mr:
                self.on_ret(ts, tid, mr)
            return
        mi = INT21.match(body)
        if mi:
            if self.active:
                self.on_int21(ts, mi.group("msg"))
            return
        mfnd = FOUND.match(body)
        if mfnd:
            self.last_found = mfnd.group("short") or mfnd.group("long")
            return
        ml = LOADLIST.match(body)
        if ml:
            self.filters[ml.group("key")] = ml.group("val")

    def on_winedbg(self, line: str) -> bool:
        """Collect winedbg --auto output: the exception line, register dump and backtrace."""
        if DBG_START.match(line):
            self.winedbg.append(line)
            self.dbg_part = "head"
            return True
        if self.dbg_part is None:
            return False
        if line.startswith(("Modules:", "Threads:")) or LINE.match(line):
            self.dbg_part = None
            return line.startswith(("Modules:", "Threads:"))
        if line in ("Register dump:", "Stack dump:", "Backtrace:"):
            self.dbg_part = line
            if line != "Stack dump:":
                self.winedbg.append(line)
        elif self.dbg_part == "Register dump:" and line.startswith(" "):
            self.winedbg.append(line)
        elif self.dbg_part == "Backtrace:" and (DBG_FRAME.match(line) or line.startswith("0x")):
            self.winedbg.append(line)
        return True

    def on_call(self, ts, tid, mc) -> None:
        mod, name, cs, ip, ds = mc.group("mod"), mc.group("name"), mc.group("cs"), mc.group("ip"), mc.group("ds")
        api = f"{mod}.{name}"
        if ds:
            self.ds_by_cs[cs][ds] += 1
        args = split_args(mc.group("args"))
        frame = Frame(ts, f"{mod}.{mc.group('ord')}", name, args, cs, ip, self.active)
        st = self.stacks[tid]
        st.append(frame)
        if len(st) > 512:  # calls that never return (Throw, process exit) must not pile up
            del st[0]
        self.recent.append(f"{name}({','.join(a.split(' ')[0] for a in args)}) @{self.where(cs, ip)}")
        if name == "LOADLIBRARY" and args:  # the self-loader runs inside this call
            self.last_loadlib = pathlib.PureWindowsPath(sval(args[0])).name.upper()
        if name == "ALLOCCSTODSALIAS" and args and args[0] == cs and cs not in self.loader_cs:
            mod_ = {"name": self.last_loadlib, "loader": cs, "sel": {cs: 1}, "next": 2}
            self.loader_cs[cs] = mod_
            self.modules.append(mod_)
        if not frame.active:
            if name == "DESTROYWINDOW" and args:
                self.forget_windows({args[0][-4:]})
            return
        self.counts[api] += 1
        at = f"@{self.where(cs, ip)}"
        if name in INI_WRITE:
            if name == "WRITEPRIVATEPROFILESTRING":
                sec, key, val, fil = (sval(a) for a in (args + ["?"] * 4)[:4])
            else:
                sec, key, val = (sval(a) for a in (args + ["?"] * 3)[:3])
                fil = "WIN.INI"
            self.ini_writes.append(f"{self.t(ts)} [{sec}] {key}={val} -> {fil}")
        elif name == "SETTIMER" and len(args) >= 3:
            ms = hexint(args[2])
            self.push_event(frame, self.timers, f"{self.t(ts)} SetTimer(hwnd={args[0]}, id={args[1]}, {ms} ms, "
                                                f"proc={args[3] if len(args) > 3 else '?'}) {at}")
        elif name == "KILLTIMER" and len(args) >= 2:
            self.timers.append(f"{self.t(ts)} KillTimer(hwnd={args[0]}, id={args[1]}) {at}")
        elif name == "TIMESETEVENT" and len(args) >= 5:
            delay, res, flags = hexint(args[0]), hexint(args[1]), hexint(args[4])
            kind = "periodic" if flags is not None and flags & 1 else "one-shot"
            self.push_event(frame, self.timers, f"{self.t(ts)} timeSetEvent({delay} ms, resolution {res} ms, "
                                                f"proc={args[2]}, {kind}) {at}")
        elif name in MM_TIMERS:
            self.push_event(frame, self.timers, f"{self.t(ts)} {name.lower()}({', '.join(args)}) {at}")
        elif name in ("CREATEWINDOW", "CREATEWINDOWEX"):
            a = args[1:] if name == "CREATEWINDOWEX" else args
            if len(a) >= 7:
                x, y, w, h = (signed16(hexint(v)) for v in a[3:7])
                ex = f" ex={args[0]}" if name == "CREATEWINDOWEX" else ""
                self.push_event(frame, self.windows,
                                f"{self.t(ts)} class \"{sval(a[0])}\" title \"{sval(a[1])}\" "
                                f"style={a[2]}{ex} x={x} y={y} w={w} h={h} "
                                f"parent={a[7] if len(a) > 7 else '?'} {at}")
        elif name == "DESTROYWINDOW" and args:
            self.geometry.append(f"{self.t(ts)} DestroyWindow {self.label(args[0][-4:])} {at}")
            self.forget_windows({args[0][-4:]})
        elif name in ("MOVEWINDOW", "SETWINDOWPOS") and len(args) >= (5 if name == "MOVEWINDOW" else 7):
            rect = args[1:5] if name == "MOVEWINDOW" else args[2:6]
            x, y, w, h = (signed16(hexint(v)) for v in rect)
            what = "MoveWindow" if name == "MOVEWINDOW" else f"SetWindowPos(after={args[1]}, flags={args[6]})"
            self.geometry.append(f"{self.t(ts)} {what} {self.label(args[0][-4:])} x={x} y={y} w={w} h={h} {at}")
        elif name in ("DIALOGBOX", "DIALOGBOXPARAM", "CREATEDIALOG", "CREATEDIALOGPARAM") and len(args) >= 2:
            self.push_event(frame, self.dialogs, f"{self.t(ts)} {name}(inst={args[0]}, template={sval(args[1])}, "
                                                 f"owner={args[2] if len(args) > 2 else '?'}) {at}")
        elif name == "MESSAGEBOX":
            text = sval(args[1]) if len(args) > 1 else "?"
            cap = sval(args[2]) if len(args) > 2 else "?"
            self.push_event(frame, self.msgboxes,
                            f"{self.t(ts)} \"{text}\" (caption \"{cap}\", flags {args[3] if len(args) > 3 else '?'}) {at}")
        elif name in FILE_APIS:
            shown = ", ".join(sval(a) for a in args)
            self.push_event(frame, self.fileops, f"{self.t(ts)} {name}({shown}) {at}")
        if name in RES_APIS and len(args) > RES_APIS[name][0]:
            idx, typ = RES_APIS[name]
            if typ is None:
                typ = res_type(args[2]) if len(args) > 2 else "?"
            frame.res_key = (args[0].split()[0][-4:], typ, res_id(args[idx]))
            self.resources.setdefault(frame.res_key, [0, self.t(ts), at, 0])[0] += 1

    def on_ret(self, ts, tid, mr) -> None:
        name, rv = mr.group("name"), mr.group("rv")
        frame = self.pop_frame(tid, f"{mr.group('mod')}.{mr.group('ord')}", mr.group("cs"), mr.group("ip"))
        v = rvint(rv)
        if name == "MYALLOC":
            m = self.loader_cs.get(mr.group("cs"))
            if m and v is not None:
                sel = f"{(v >> 16) | 1:04x}"
                m["sel"][sel] = m["next"]
                m["next"] += 1
            return
        if frame is None:
            return
        args = frame.args
        if name in ("LOADLIBRARY", "GETMODULEHANDLE") and args and v is not None and v & 0xffff >= 32:
            self.instances[f"{v & 0xffff:04x}"] = pathlib.PureWindowsPath(sval(args[0])).name.upper()
        if name in ("CREATEWINDOW", "CREATEWINDOWEX"):
            a = args[1:] if name == "CREATEWINDOWEX" else args
            if v and v & 0xffff and a:
                self.hwnds[f"{v & 0xffff:04x}"] = (sval(a[0]), a[7][-4:] if len(a) > 7 else "0000")
        elif name in ("DIALOGBOX", "DIALOGBOXPARAM"):
            # The dialog and its controls are gone, and so are the windows created inside it.
            self.forget_windows({h for h, (_, parent) in self.hwnds.items()
                                 if parent != "0000" and parent not in self.hwnds})
        if not frame.active:
            return
        if frame.res_key is not None and v == 0:
            self.resources[frame.res_key][3] += 1
        if name in INI_READ and len(args) >= 2:
            if name.startswith("GETPRIVATE"):
                sec, key, fil = sval(args[0]), sval(args[1]), sval(args[-1])
            else:
                sec, key, fil = sval(args[0]), sval(args[1]), "WIN.INI"
            default = sval(args[2]) if len(args) > 2 else ""
            k = (fil, sec, key)
            if v is None:
                res = "none"
            elif name.endswith("INT"):
                res = f"{v & 0xffff}"
            else:
                res = f"len {v & 0xffff}"
            if k not in self.ini_reads:
                self.ini_reads[k] = [0, default, res, name.endswith("INT")]
                self.ini_order.append(k)
            self.ini_reads[k][0] += 1
        elif name in ("GETDEVICECAPS", "GETSYSTEMMETRICS"):
            idx = hexint(args[-1]) if args else None
            self.devcaps.setdefault((name, idx), "none" if v is None else f"{v & 0xffff}")
        elif name in ("CREATEWINDOW", "CREATEWINDOWEX"):
            self.complete(frame, " -> none" if v is None else f" -> hwnd {v & 0xffff:04x}")
        elif frame.slot is not None:
            self.complete(frame, f" -> {rvtext(rv)}")

    def on_int21(self, ts, msg: str) -> None:
        if msg.startswith("AX="):
            return
        if msg.startswith("returning:"):
            if self.search is not None and self.search.get("await"):
                efl = dict(REGS.findall(msg)).get("EFL", "0")
                if not int(efl, 16) & 1 and self.last_found is not None:
                    self.search["found"].append(self.last_found)
                self.search["await"] = False
            self.fallthrough = False
            return
        if msg.startswith("SET DISK TRANSFER AREA ADDRESS"):
            self.dta = msg.rsplit(" ", 1)[-1]
            return
        if msg.startswith("GET DISK TRANSFER AREA"):
            return
        if msg.startswith(("SELECT DEFAULT DRIVE", "GET CURRENT DRIVE", "Failed to set current drive")):
            if self.probe is None:
                self.probe = {"t": self.t(ts), "tried": [], "ok": []}
                self.fileops.append(self.probe)
            if msg.startswith("SELECT DEFAULT DRIVE"):
                self.probe["tried"].append(msg[-2])
                self.probe["sel"] = msg[-2]
            elif msg.startswith("GET CURRENT DRIVE") and self.probe.get("sel"):
                if msg[-2] == self.probe["sel"]:
                    self.probe["ok"].append(msg[-2])
                self.probe["sel"] = None
            return
        self.probe = None
        if msg.startswith("FINDFIRST"):
            mm = re.match(r'FINDFIRST mask 0x([0-9a-f]+) spec "(.*)"', msg)
            spec = mm.group(2).replace("\\\\", "\\") if mm else msg
            self.search = {"t": self.t(ts), "spec": spec, "mask": mm.group(1) if mm else "?",
                           "dta": self.dta, "found": [], "await": True, "calls": 1}
            self.by_dta[self.dta] = self.search
            self.fileops.append(self.search)
            self.fallthrough = True
            self.last_found = None
            return
        if msg.startswith("FINDNEXT"):
            if self.fallthrough:
                return  # the FINDNEXT half of FINDFIRST
            self.search = self.by_dta.get(self.dta)
            if self.search is not None:
                self.search["calls"] += 1
                self.search["await"] = True
            self.last_found = None
            return
        if msg.startswith("returning ") and msg.endswith("as label"):
            self.last_found = "label " + msg.split(" ", 2)[1]
            return
        if msg.startswith("failed"):
            return
        if any(msg.startswith(k) for k in INT21_KEEP) and not msg.startswith(("LSEEK", "READ", "WRITE")):
            self.fileops.append(f"{self.t(ts)} int21 {msg}")

    # --- output --------------------------------------------------------------
    def render(self, src: str, maxn: int) -> str:
        o: list[str] = []
        span = f"{self.t1 - self.t0:.1f} s" if self.t0 is not None and self.t1 is not None else "?"
        o.append(f"# Relay digest: {src}\n")
        o.append(f"- {self.lines} lines, {span} of trace; times below are seconds from the first line.")
        if self.start_re is not None or self.until_re is not None:
            w0, w1 = (self.t(x).strip() if x else "?" for x in self.inside)
            o.append(f"- Events limited to lines from `{self.start_re.pattern if self.start_re else 'start'}` "
                     f"to `{self.until_re.pattern if self.until_re else 'end'}` ({w0} s to {w1} s).")
        for k in ("RelayInclude", "RelayExclude"):
            if k in self.filters:
                o.append(f"- {k}: `{self.filters[k]}`")
        o.append("")
        o.append("## Selector map (OPTLOADER modules; segN from MYALLOC order)\n")
        for m in self.modules:
            sels = ", ".join(f"seg{n}={s}" for s, n in sorted(m["sel"].items(), key=lambda kv: kv[1]))
            o.append(f"- {m['name']}: {sels}; DGROUP selector (most common ds) {self.dgroup(m) or '?'}")
        o.append("")
        o.append("## Instance handles\n")
        inst = {h: n for h, n in self.instances.items() if h != "0000"}
        for m in self.modules:
            ds = self.dgroup(m)
            if ds:
                inst.setdefault(f"{int(ds, 16) & 0xfffe:04x}", m["name"])
        o.append("    " + (", ".join(f"{h} {n}" for h, n in sorted(inst.items())) or "none"))
        o.append("")
        o.append("## 16-bit API call counts\n")
        bymod: dict[str, list] = collections.defaultdict(list)
        for api, n in self.counts.items():
            bymod[api.split(".")[0]].append((n, api.split(".")[1]))
        for mod in sorted(bymod):
            items = sorted(bymod[mod], reverse=True)
            o.append(f"- **{mod}** ({sum(n for n, _ in items)}): " + ", ".join(f"{a} {n}" for n, a in items))
        o.append("")
        o.append("## File and directory operations (in order)\n")
        rendered = []
        for ev in self.fileops:
            if isinstance(ev, dict) and "tried" in ev:
                rendered.append(f"{ev['t']} int21 drive probe (select + get current drive): "
                                f"tried {''.join(ev['tried'])}, accepted {''.join(ev['ok']) or 'none'}")
            elif isinstance(ev, dict):
                names = ev["found"]
                shown = ", ".join(names[:12]) + (f", ... ({len(names) - 12} more)" if len(names) > 12 else "")
                rendered.append(f"{ev['t']} FINDFIRST \"{ev['spec']}\" attr 0x{ev['mask']} dta {ev['dta']}: "
                                f"{ev['calls']} call(s), {len(names)} found [{shown}]")
            else:
                rendered.append(ev)
        # collapse identical consecutive lines (e.g. drive probes)
        coll: list[str] = []
        for r in rendered:
            key = re.sub(r"^\s*[\d.]+ ", "", r)
            if coll and re.sub(r"^\s*[\d.]+ ", "", coll[-1]).split(" (x")[0] == key:
                base, _, n = coll[-1].partition(" (x")
                coll[-1] = f"{base} (x{int(n.rstrip(')') or 1) + 1})"
            else:
                coll.append(r)
        o.extend(f"    {c}" for c in coll[:maxn])
        if len(coll) > maxn:
            o.append(f"    ... {len(coll) - maxn} more")
        o.append("")
        o.append("## INI reads (first occurrence order; count, default, first result)\n")
        for k in self.ini_order[:maxn]:
            n, d, res, _ = self.ini_reads[k]
            o.append(f"- {k[0]} [{k[1]}] {k[2]}: x{n}, default \"{d}\", {res}")
        if len(self.ini_order) > maxn:
            o.append(f"- ... {len(self.ini_order) - maxn} more")

        def section(title: str, items: list[str]) -> None:
            o.append(f"\n## {title}\n")
            o.extend(f"    {x}" for x in items[:maxn]) if items else o.append("    none")
            if len(items) > maxn:
                o.append(f"    ... {len(items) - maxn} more")

        section("INI writes", self.ini_writes)
        section("Timers", self.timers)
        section("Windows created", self.windows)
        section("Window geometry (MoveWindow, SetWindowPos, DestroyWindow)", self.geometry)
        section("Dialogs", self.dialogs)
        section("MessageBox texts", self.msgboxes)
        o.append("\n## Resources (per module and type: name xcount @first time; return addresses)\n")
        groups: dict[tuple, list] = {}
        for (hinst, typ, name), (n, t, at, failed) in self.resources.items():
            groups.setdefault((self.module_of(hinst), typ), []).append((name, n, t.strip(), at, failed))
        for (mod, typ), ents in groups.items():
            names = [nm + (f" x{n}" if n > 1 else "") + f" @{t}"
                     + (f" (failed x{f})" if f else "") for nm, n, t, _, f in ents]
            sites = sorted({e[3] for e in ents})
            o.append(f"- {mod} {typ} ({len(ents)}): " + ", ".join(names[:maxn])
                     + (f", ... {len(names) - maxn} more" if len(names) > maxn else "")
                     + f"; {' '.join(sites[:8])}" + (" ..." if len(sites) > 8 else ""))
        if not groups:
            o.append("    none")
        o.append("\n## GetDeviceCaps / GetSystemMetrics (index: first answer)\n")
        dc = [f"{n[3:]}[{i}]={v}" for (n, i), v in sorted(self.devcaps.items(), key=lambda kv: (kv[0][0], kv[0][1] or 0))]
        o.append("    " + (", ".join(dc) if dc else "none"))
        o.append("\n## Faults\n")
        o.extend(f"    {x}" for x in self.faults) if self.faults else o.append("    none")
        if self.winedbg:
            o.append("\n    winedbg:")
            o.extend(f"    {x}" for x in self.winedbg)
        return "\n".join(o) + "\n"


def digest_lines(lines, start: str | None = None, until: str | None = None) -> Digest:
    d = Digest(start, until)
    for line in lines:
        d.feed(line)
    return d


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=pathlib.Path)
    ap.add_argument("-o", "--out", type=pathlib.Path)
    ap.add_argument("--max-list", type=int, default=400)
    ap.add_argument("--from", dest="start", metavar="REGEX",
                    help="record events from the first line matching REGEX")
    ap.add_argument("--until", metavar="REGEX",
                    help="stop recording events at the first later line matching REGEX")
    a = ap.parse_args(argv)
    with a.log.open("r", encoding="latin-1", errors="replace") as fh:
        d = digest_lines(fh, a.start, a.until)
    text = d.render(a.log.name, a.max_list)
    if a.out:
        a.out.write_text(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
