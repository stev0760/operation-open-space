#!/usr/bin/env python3
"""fly_redact.py PREFIX DIR: replace the pilot name and the organisation in the text files under DIR
(trace.log, watch-*.txt, steps.txt, timeline.txt, INSPACE.INI ...) by <redacted>.

The values are read at run time from the prefix's INSPACE.INI (ThePlayer) and INSPACE.EXE (the
"name - organisation" registration string), and are never printed. Binary files (saves, .ded, video,
pictures) are left alone: they stay in work/, which is not committed.
"""
import os, re, sys

prefix, root = sys.argv[1], sys.argv[2]
ini = open(os.path.join(prefix, "drive_c", "windows", "INSPACE.INI"), encoding="latin-1").read()
vals = set()
m = re.search(r"^ThePlayer=(.*)$", ini, re.M)
name = m.group(1).strip() if m else ""
if len(name) >= 3:
    vals.add(name)
    exe = os.path.join(prefix, "drive_c", "SPACE", "INSPACE.EXE")
    if os.path.exists(exe):
        b = open(exe, "rb").read()
        mm = re.search(re.escape(name.encode("latin-1")) + rb" - ([^\x00]{1,22})", b)
        if mm:
            vals.add(mm.group(1).decode("latin-1").strip())
vals = {v for v in vals if len(v) >= 3}
n = 0
for dp, _, fs in os.walk(root):
    for f in fs:
        if not f.endswith((".txt", ".log", ".out", ".INI", ".ini", ".md", ".err", ".tsv", ".before")):
            continue
        p = os.path.join(dp, f)
        try:
            b = open(p, "rb").read()
        except OSError:
            continue
        o = b
        for v in vals:
            o = re.sub(re.escape(v.encode("latin-1")), b"<redacted>", o, flags=re.I)
        if o != b:
            open(p, "wb").write(o)
            n += 1
print(f"fly_redact: files changed {n}, values {len(vals)}")
