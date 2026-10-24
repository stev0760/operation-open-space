#!/usr/bin/env python3
"""Summarise a Wine +relay/+int21/+file trace of the game into a small markdown digest.

Usage: relay_digest.py TRACE.log [-o DIGEST.md] [--max-list N]

Input is a log written with oracle/trace.sh (WINEDEBUG=+relay,+int21,+file,+timestamp
and the 16-bit RelayInclude filter). Sections:
  - relay filter in effect (from the log's own load_list lines)
  - selector -> segN map for OPTLOADER modules (INSPACE.EXE, ISPACE.DLL), built from
    the self-loader's KERNEL.MYALLOC calls: the k-th MYALLOC of a module allocates
    segment k+1 (segment 1 is the loader itself, whose CS calls ALLOCCSTODSALIAS)
  - per-16-bit-API call counts
  - ordered file and directory operations, with the entries each FINDFIRST/FINDNEXT
    delivered (from kernelbase FindNextFileW "returning" lines)
  - INI reads and writes, timers, windows and dialogs, MessageBox texts
  - GetDeviceCaps/GetSystemMetrics answers, faults, oracle shim messages
Return addresses are printed as segN:0xOFF where the selector map knows the CS,
otherwise as the raw selector:offset. They are return addresses (after the call).
"""
from __future__ import annotations

import argparse
import collections
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

FILE_APIS = {"OPENFILE", "_LOPEN", "_LCREAT", "LOADLIBRARY", "LOADMODULE", "WINEXEC",
             "EXTRACTICON", "GETMODULEFILENAME", "GETWINDOWSDIRECTORY", "GETSYSTEMDIRECTORY",
             "GETDRIVETYPE", "GETTEMPFILENAME", "DELETEFILE", "SHELLEXECUTE", "FINDEXECUTABLE"}
INI_READ = {"GETPRIVATEPROFILESTRING", "GETPRIVATEPROFILEINT", "GETPROFILESTRING", "GETPROFILEINT"}
INI_WRITE = {"WRITEPRIVATEPROFILESTRING", "WRITEPROFILESTRING"}
INT21_KEEP = ("OPEN", "CREAT", "FINDFIRST", "FINDNEXT", "failed", "CHANGE CURRENT DIRECTORY",
              "MKDIR", "RMDIR", "UNLINK", "DELETE", "RENAME", "GET CURRENT DIRECTORY",
              "SELECT DEFAULT DRIVE", "GET CURRENT DRIVE", "returning", "GET FILE ATTRIBUTES",
              "SET FILE ATTRIBUTES", "Failed to set current drive", "GET FREE DISK")


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


class Digest:
    def __init__(self) -> None:
        self.t0: float | None = None
        self.t1: float | None = None
        self.lines = 0
        self.filters: dict[str, str] = {}
        self.counts: collections.Counter[str] = collections.Counter()
        self.pending: dict[str, tuple] = {}  # tid -> last call (for retval pairing)
        # self-loader selector map
        self.modules: list[dict] = []
        self.loader_cs: dict[str, dict] = {}
        self.last_loadlib = "INSPACE.EXE"
        self.ds_by_cs: dict[str, collections.Counter[str]] = collections.defaultdict(collections.Counter)
        # event lists
        self.fileops: list[str] = []
        self.ini_reads: dict[tuple, list] = {}
        self.ini_order: list[tuple] = []
        self.ini_writes: list[str] = []
        self.timers: list[str] = []
        self.windows: list[str] = []
        self.dialogs: list[str] = []
        self.msgboxes: list[str] = []
        self.devcaps: dict[tuple, str] = {}
        self.faults: list[str] = []
        self.shim: collections.Counter[str] = collections.Counter()
        self.recent: collections.deque[str] = collections.deque(maxlen=6)
        # findfirst/findnext pairing: DOS keeps each search's state in its DTA
        self.dta = "default"
        self.by_dta: dict[str, dict] = {}
        self.search: dict | None = None  # search the current int21 call works on
        self.fallthrough = False  # FINDFIRST logs its own FINDNEXT half
        self.last_found: str | None = None
        self.probe: dict | None = None  # SELECT DEFAULT DRIVE / GET CURRENT DRIVE run

    # --- helpers -------------------------------------------------------------
    def t(self, ts: str | None) -> str:
        if ts is None or self.t0 is None:
            return "      "
        return f"{float(ts) - self.t0:6.1f}"

    def where(self, cs: str, ip: str) -> str:
        for m in self.modules:
            if cs in m["sel"]:
                return f"{m['name']} seg{m['sel'][cs]}:0x{ip}"
        return f"{cs}:{ip}"

    # --- line handlers -------------------------------------------------------
    def feed(self, raw: str) -> None:
        self.lines += 1
        line = raw.rstrip("\n")
        if line.startswith("inspace-shim:"):
            self.shim[re.sub(r"0x[0-9a-f]+|[0-9a-f]{8}", "#", line)] += 1
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
            self.on_int21(ts, mi.group("msg"))
            return
        mfnd = FOUND.match(body)
        if mfnd:
            self.last_found = mfnd.group("short") or mfnd.group("long")
            return
        ml = LOADLIST.match(body)
        if ml:
            self.filters[ml.group("key")] = ml.group("val")

    def on_call(self, ts, tid, mc) -> None:
        mod, name, cs, ip, ds = mc.group("mod"), mc.group("name"), mc.group("cs"), mc.group("ip"), mc.group("ds")
        api = f"{mod}.{name}"
        self.counts[api] += 1
        if ds:
            self.ds_by_cs[cs][ds] += 1
        args = split_args(mc.group("args"))
        self.pending[tid] = (ts, api, args, cs, ip)
        self.recent.append(f"{name}({','.join(a.split(' ')[0] for a in args)}) @{self.where(cs, ip)}")
        if name == "LOADLIBRARY" and args:  # the self-loader runs inside this call
            self.last_loadlib = pathlib.PureWindowsPath(sval(args[0])).name.upper()
        if name == "ALLOCCSTODSALIAS" and args and args[0] == cs and cs not in self.loader_cs:
            mod_ = {"name": self.last_loadlib, "loader": cs, "sel": {cs: 1}, "next": 2}
            self.loader_cs[cs] = mod_
            self.modules.append(mod_)
        if name in INI_WRITE:
            if name == "WRITEPRIVATEPROFILESTRING":
                sec, key, val, fil = (sval(a) for a in args[:4])
            else:
                sec, key, val = (sval(a) for a in args[:3])
                fil = "WIN.INI"
            self.ini_writes.append(f"{self.t(ts)} [{sec}] {key}={val} -> {fil}")
        elif name == "SETTIMER":
            ms = hexint(args[2]) if len(args) > 2 else None
            self.timers.append(f"{self.t(ts)} SetTimer(hwnd={args[0]}, id={args[1]}, {ms} ms, "
                               f"proc={args[3] if len(args) > 3 else '?'}) @{self.where(cs, ip)}")
        elif name == "KILLTIMER":
            self.timers.append(f"{self.t(ts)} KillTimer(hwnd={args[0]}, id={args[1]}) @{self.where(cs, ip)}")
        elif name in ("CREATEWINDOW", "CREATEWINDOWEX"):
            a = args[1:] if name == "CREATEWINDOWEX" else args
            if len(a) >= 7:
                x, y, w, h = (hexint(v) for v in a[3:7])
                sx = lambda v: v - 0x10000 if v is not None and v >= 0x8000 else v  # noqa: E731
                ex = f" ex={args[0]}" if name == "CREATEWINDOWEX" else ""
                self.windows.append(f"{self.t(ts)} class \"{sval(a[0])}\" title \"{sval(a[1])}\" "
                                    f"style={a[2]}{ex} x={sx(x)} y={sx(y)} w={sx(w)} h={sx(h)} "
                                    f"parent={a[7] if len(a) > 7 else '?'} @{self.where(cs, ip)}")
        elif name in ("DIALOGBOX", "DIALOGBOXPARAM", "CREATEDIALOG", "CREATEDIALOGPARAM"):
            self.dialogs.append(f"{self.t(ts)} {name}(inst={args[0]}, template={sval(args[1])}, "
                                f"owner={args[2] if len(args) > 2 else '?'}) @{self.where(cs, ip)}")
        elif name == "MESSAGEBOX":
            text = sval(args[1]) if len(args) > 1 else "?"
            cap = sval(args[2]) if len(args) > 2 else "?"
            self.msgboxes.append(f"{self.t(ts)} \"{text}\" (caption \"{cap}\", flags {args[3] if len(args) > 3 else '?'})"
                                 f" @{self.where(cs, ip)}")
        elif name in FILE_APIS:
            shown = ", ".join(sval(a) for a in args)
            self.fileops.append(f"{self.t(ts)} {name}({shown}) @{self.where(cs, ip)}")

    def on_ret(self, ts, tid, mr) -> None:
        name, rv = mr.group("name"), mr.group("rv")
        call = self.pending.pop(tid, None)
        if name == "MYALLOC":
            m = self.loader_cs.get(mr.group("cs"))
            if m and rv != "none":
                sel = f"{(int(rv, 16) >> 16) | 1:04x}"
                m["sel"][sel] = m["next"]
                m["next"] += 1
            return
        if not call:
            return
        _, api, args, cs, ip = call
        if name in INI_READ:
            if name.startswith("GETPRIVATE"):
                sec, key = sval(args[0]), sval(args[1])
                fil = sval(args[-1])
            else:
                sec, key, fil = sval(args[0]), sval(args[1]), "WIN.INI"
            default = sval(args[2]) if len(args) > 2 else ""
            k = (fil, sec, key)
            res = f"{int(rv, 16)}" if name.endswith("INT") and rv != "none" else f"len {int(rv, 16)}"
            if k not in self.ini_reads:
                self.ini_reads[k] = [0, default, res, name.endswith("INT")]
                self.ini_order.append(k)
            self.ini_reads[k][0] += 1
        elif name in ("GETDEVICECAPS", "GETSYSTEMMETRICS"):
            idx = hexint(args[-1])
            self.devcaps.setdefault((name, idx), f"{int(rv, 16) & 0xffff}" if rv != "none" else rv)
        elif name in FILE_APIS and self.fileops:
            self.fileops[-1] += f" -> {rv}"
        elif name in ("CREATEWINDOW", "CREATEWINDOWEX") and self.windows:
            self.windows[-1] += f" -> hwnd {int(rv, 16) & 0xffff:04x}"
        elif name in ("DIALOGBOX", "DIALOGBOXPARAM") and self.dialogs:
            self.dialogs[-1] += f" -> {rv}"
        elif name == "SETTIMER" and self.timers:
            self.timers[-1] += f" -> {rv}"

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
        for k in ("RelayInclude", "RelayExclude"):
            if k in self.filters:
                o.append(f"- {k}: `{self.filters[k]}`")
        o.append("")
        o.append("## Selector map (OPTLOADER modules; segN from MYALLOC order)\n")
        for m in self.modules:
            dss = collections.Counter()
            for cs in m["sel"]:
                dss.update(self.ds_by_cs.get(cs, {}))
            ds = dss.most_common(1)[0][0] if dss else "?"
            sels = ", ".join(f"seg{n}={s}" for s, n in sorted(m["sel"].items(), key=lambda kv: kv[1]))
            o.append(f"- {m['name']}: {sels}; DGROUP selector (most common ds) {ds}")
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
        o.append("\n## INI writes\n")
        o.extend(f"    {w}" for w in self.ini_writes[:maxn]) if self.ini_writes else o.append("    none")
        o.append("\n## Timers\n")
        o.extend(f"    {x}" for x in self.timers[:maxn]) if self.timers else o.append("    none")
        o.append("\n## Windows created\n")
        o.extend(f"    {x}" for x in self.windows[:maxn]) if self.windows else o.append("    none")
        o.append("\n## Dialogs\n")
        o.extend(f"    {x}" for x in self.dialogs[:maxn]) if self.dialogs else o.append("    none")
        o.append("\n## MessageBox texts\n")
        o.extend(f"    {x}" for x in self.msgboxes) if self.msgboxes else o.append("    none")
        o.append("\n## GetDeviceCaps / GetSystemMetrics (index: first answer)\n")
        dc = [f"{n[3:]}[{i}]={v}" for (n, i), v in sorted(self.devcaps.items(), key=lambda kv: (kv[0][0], kv[0][1] or 0))]
        o.append("    " + (", ".join(dc) if dc else "none"))
        o.append("\n## Faults\n")
        o.extend(f"    {x}" for x in self.faults) if self.faults else o.append("    none")
        o.append("\n## Oracle shim\n")
        o.extend(f"    {s} (x{n})" for s, n in self.shim.items()) if self.shim else o.append("    none")
        return "\n".join(o) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=pathlib.Path)
    ap.add_argument("-o", "--out", type=pathlib.Path)
    ap.add_argument("--max-list", type=int, default=400)
    a = ap.parse_args()
    d = Digest()
    with a.log.open("r", encoding="latin-1", errors="replace") as fh:
        for line in fh:
            d.feed(line)
    text = d.render(a.log.name, a.max_list)
    if a.out:
        a.out.write_text(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
