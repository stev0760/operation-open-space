#!/usr/bin/env python3
"""fly_timeline.py RUNDIR: one merged timeline on the wall clock, written to stdout.

Lines: step log (steps.txt), the watchers' changes (decoded as little-endian words, a dword for
4-byte spans), and from trace.log the sound plays, message boxes, dialogs, string loads, INI writes
and file opens, and the text the game draws (TEXTOUT; a string is printed when it first appears and
again after 2 s without it, so the message lines of the wave and the Hall of Justice text show once).
Trace time (seconds on the monotonic clock) becomes wall time with clock-offset.txt, which fly.sh
writes at the start (time.time() - time.monotonic()).
"""
import datetime as dt, os, re, sys

R = sys.argv[1]
off = float(open(os.path.join(R, "clock-offset.txt")).read().split()[0]) if os.path.exists(os.path.join(R, "clock-offset.txt")) else None


def wall(x):
    return dt.datetime.fromtimestamp(x).strftime("%H:%M:%S.%f")[:-3]


def secs(hms):
    h, m, s = hms.split(":")
    return int(h) * 3600 + int(m) * 60 + float(s)


ev = []   # (seconds of the day, text)
day0 = None
sp = os.path.join(R, "steps.txt")
if os.path.exists(sp):
    for l in open(sp, errors="replace"):
        w = l.split(" ", 1)
        if len(w) == 2 and re.match(r"\d\d:\d\d:\d\d\.\d+", w[0]) and " shot " not in l:
            ev.append((secs(w[0]), "step " + w[1].strip()[:160]))
for fn in sorted(os.listdir(R)):
    m = re.fullmatch(r"watch-(.+)\.txt", fn)
    if not m:
        continue
    name = m.group(1)
    for l in open(os.path.join(R, fn), errors="replace"):
        w = l.split()
        if len(w) < 6 or not re.match(r"\d\d:\d\d:", w[0]):
            continue
        try:
            k = w.index("init") if "init" in w else w.index("chg")
        except ValueError:
            continue
        b = [int(x, 16) for x in w[k + 1:] if re.fullmatch(r"[0-9a-f]{2}", x)]
        if len(b) == 4:
            val = str(int.from_bytes(bytes(b), "little", signed=True))
        elif len(b) > 1:
            val = " ".join(str(int.from_bytes(bytes(b[i:i + 2]), "little", signed=True)) for i in range(0, len(b) - 1, 2))
        else:
            val = " ".join(map(str, b))
        ev.append((secs(w[0]), f"watch {name} {w[k]} {val}"))
tp = os.path.join(R, "trace.log")
if os.path.exists(tp) and off is not None:
    last = {}
    pend = None
    pat = re.compile(r'^(\d+\.\d+):\w+:Call (\S+): (\w+)\((.*)')
    with open(tp, errors="replace") as f:
        for l in f:
            if "Call " not in l[:40] and ":Call " not in l[:30]:
                continue
            m = pat.match(l)
            if not m:
                continue
            ts, api, fn, args = float(m.group(1)), m.group(2), m.group(3), m.group(4)
            d = dt.datetime.fromtimestamp(ts + off)
            sod = d.hour * 3600 + d.minute * 60 + d.second + d.microsecond / 1e6
            if fn == "TEXTOUT":
                s = re.search(r'"(.*)",[0-9a-f]+\)', args)
                if not s or not s.group(1).strip():
                    continue
                t = s.group(1)
                # a drawn line is shown once: growing (typewriter), single characters of one
                # line, and a marquee (a suffix of the shown text) are merged into one event
                if pend and ts - pend[2] < 1.5:
                    pt = pend[1]
                    if len(t) == 1:
                        pend[1] = pt + t; pend[2] = ts; continue
                    if t.startswith(pt) or pt.startswith(t) or pt.endswith(t):
                        if len(t) > len(pt) and t.startswith(pt):
                            pend[1] = t
                        pend[2] = ts
                        continue
                if t in last and ts - last[t] < 2.0:
                    last[t] = ts
                    continue
                if pend:
                    ev.append((pend[0], f"text {pend[1][:200]}"))
                last[t] = ts
                pend = [sod, t, ts]
            elif fn in ("SNDPLAYSOUND", "MESSAGEBOX", "DIALOGBOX", "LOADSTRING", "WRITEPRIVATEPROFILESTRING", "OPENFILE", "_LCREAT", "MIXPLAY", "WAVEOUTOPEN"):
                if fn == "LOADSTRING":
                    mm = re.match(r"\w+,(\w+),", args)
                    ev.append((sod, f"LOADSTRING id {int(mm.group(1), 16)}" if mm else "LOADSTRING"))
                else:
                    ev.append((sod, f"trace {api}: {fn}({args[:140]}"))
if pend:
    ev.append((pend[0], f"text {pend[1][:200]}"))
ev.sort()
print(f"# clock offset {off}; {len(ev)} events")
for t, s in ev:
    h = int(t // 3600); mi = int(t % 3600 // 60); sec = t % 60
    print(f"{h:02d}:{mi:02d}:{sec:06.3f} {s}")
