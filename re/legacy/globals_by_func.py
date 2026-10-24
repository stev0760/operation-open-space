#!/usr/bin/env python3
"""Attribute references to DGROUP absolute globals to their enclosing function.

The disassembler renders a DGROUP absolute address in two ways depending on the
instruction form:
    mov 0x9348,%ax      (moffs form)      -> unsigned 0xNNNN
    mov -0x6ce8,%cx     (modrm disp16)    -> SIGNED, i.e. 0x9318
so every global must be searched for as BOTH 0xNNNN and -(0x10000-NNNN).

usage: globals_by_func.py 9e60 5078 1e58 94a6 ...
"""
import json, glob, os, re, sys, collections

want = [int(x, 16) for x in sys.argv[1:]] or [0x9e60, 0x5078, 0x1e58, 0x94a6]

ent = {int(k): sorted(v) for k, v in json.load(open("function-entries.json")).items()}
LINE = re.compile(r'^\s*([0-9a-f]+):')

# build the two textual forms for each global
forms = {}
for g in want:
    forms[g] = {f"0x{g:x}", f"-0x{0x10000 - g:x}"}

hits = collections.defaultdict(collections.Counter)   # global -> Counter(function)
ex = collections.defaultdict(list)
loc = collections.defaultdict(list)

for path in sorted(glob.glob("annotated-asm/seg*.asm")):
    seg = int(''.join(c for c in os.path.basename(path) if c.isdigit()))
    lst = ent.get(seg, [])
    for line in open(path, encoding="utf-8", errors="replace"):
        m = LINE.match(line)
        if not m:
            continue
        off = int(m.group(1), 16)
        # enclosing function
        lo, hi, fn = 0, len(lst) - 1, None
        while lo <= hi:
            mid = (lo + hi) // 2
            if lst[mid] <= off:
                fn = lst[mid]; lo = mid + 1
            else:
                hi = mid - 1
        if fn is None:
            continue
        for g, fs in forms.items():
            if any(f in line for f in fs):
                hits[g][(seg, fn)] += 1
                if len(loc[g]) < 400:
                    loc[g].append((seg, fn, off, line.strip()[:110]))

for g in want:
    print(f"\n=== DGROUP:0x{g:04x}  (rendered as {sorted(forms[g])}) ===")
    tot = sum(hits[g].values())
    print(f"  total attributed references: {tot}")
    print("  top functions:")
    for (seg, fn), n in hits[g].most_common(10):
        print(f"    seg{seg}:{fn:#06x}  {n}")
    print("  sample lines:")
    seen = set()
    for seg, fn, off, ln in loc[g]:
        k = (seg, fn)
        if k in seen:
            continue
        seen.add(k)
        print(f"    seg{seg}:{off:#06x}  {ln}")
        if len(seen) >= 8:
            break
