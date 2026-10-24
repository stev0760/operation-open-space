#!/usr/bin/env python3
"""Attribute field accesses to the function that makes them.

Uses function-entries.json (the byte-verified prologue inventory) to find the
enclosing function for each `es:disp(%bx...)` access in the annotated disassembly,
then ranks functions by how heavily they use a given displacement. Also reports which
far-pointer GLOBAL each function loads as its struct base -- that is how the semantics
of the ~15 stride-0x70 pointers can be told apart (which code path loads which).

usage: field_by_func.py <hex_disp> [top_n]
       field_by_func.py 4
"""
import json, glob, os, re, sys, collections

disp = int(sys.argv[1], 16)
topn = int(sys.argv[2]) if len(sys.argv) > 2 else 12

entries = json.load(open("function-entries.json"))
ent = {int(s): sorted(v) for s, v in entries.items()}

LINE = re.compile(r'^\s*([0-9a-f]+):')
LES  = re.compile(r'les\s+0x([0-9a-f]+),%bx')
FLD  = re.compile('%es:0x' + format(disp, 'x') + r'\(%bx')
ANYFLD = re.compile(r'%es:0x([0-9a-f]+)\(%bx')

def enclosing(seg, off):
    lst = ent.get(seg, [])
    lo, hi, best = 0, len(lst) - 1, None
    while lo <= hi:
        mid = (lo + hi) // 2
        if lst[mid] <= off:
            best = lst[mid]; lo = mid + 1
        else:
            hi = mid - 1
    return best

hits = collections.Counter()
examples = collections.defaultdict(list)
base_use = collections.defaultdict(collections.Counter)

for path in sorted(glob.glob("annotated-asm/seg*.asm")):
    seg = int(''.join(c for c in os.path.basename(path) if c.isdigit()))
    cur_base = None
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = LINE.match(line)
            if not m:
                continue
            off = int(m.group(1), 16)
            b = LES.search(line)
            if b:
                cur_base = int(b.group(1), 16)
            if FLD.search(line):
                fn = enclosing(seg, off)
                if fn is not None:
                    key = (seg, fn)
                    hits[key] += 1
                    if len(examples[key]) < 4:
                        examples[key].append((off, line.strip()[:96]))
                    if cur_base is not None:
                        base_use[key][cur_base] += 1

print(f"=== functions ranked by es:0x{disp:x} accesses ===")
print(f"(denominator: {sum(hits.values())} attributed accesses)\n")
for (seg, fn), n in hits.most_common(topn):
    bases = ", ".join(f"0x{g:04x}x{c}" for g, c in base_use[(seg, fn)].most_common(3))
    print(f"  seg{seg}:{fn:#06x}  {n:>3} accesses   bases: {bases or '-'}")
    for off, ln in examples[(seg, fn)][:2]:
        print(f"        {off:#06x}  {ln}")

print("\n=== which far-pointer GLOBAL each of those functions loads as base ===")
for (seg, fn), n in hits.most_common(topn):
    if base_use[(seg, fn)]:
        bs = ", ".join(f"0x{g:04x}({c})" for g, c in base_use[(seg, fn)].most_common())
        print(f"  seg{seg}:{fn:#06x}  ->  {bs}")
