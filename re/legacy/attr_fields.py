#!/usr/bin/env python3
"""For each `es:disp(%bx,%si)` / `(%bx,%di)` struct access in the annotated
disassembly, attribute the field to the most recent `les <global>,%bx` load.
This separates fields that belong to DIFFERENT far-pointer structures that happen
to share a stride."""
import re, glob, os, collections

LES = re.compile(r'les\s+0x([0-9a-f]+),%bx')
FLD = re.compile(r'%es:0x([0-9a-f]+)\(%bx(?:,%s[il])?\)')

per_global = collections.defaultdict(collections.Counter)
unknown = collections.Counter()

for path in sorted(glob.glob('annotated-asm/*.asm')):
    cur = None
    with open(path, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            m = LES.search(line)
            if m:
                cur = int(m.group(1), 16)
                continue
            # a write to a global base register invalidates attribution
            if re.search(r'mov\s+0x[0-9a-f]+,%bx\b', line) or re.search(r'les\s+[^,]+,\s*%bx', line) is None and False:
                pass
            for f in FLD.finditer(line):
                disp = int(f.group(1), 16)
                if cur is None:
                    unknown[disp] += 1
                else:
                    per_global[cur][disp] += 1

print("=== struct accesses attributed to each far-pointer global ===")
for g in sorted(per_global):
    tot = sum(per_global[g].values())
    disps = sorted(per_global[g])
    print(f"\nDGROUP:0x{g:04x}  total={tot}  distinct_disps={len(disps)}")
    print(f"  disps: {' '.join(f'{d:x}' for d in disps[:40])}"
          + (" ..." if len(disps) > 40 else ""))
    print("  top:   " + ", ".join(f"+0x{d:x}({c})" for d, c in per_global[g].most_common(14)))

print("\n=== accesses with no preceding les (not attributable) ===")
print("  " + ", ".join(f"+0x{d:x}({c})" for d, c in unknown.most_common(20)))

# focus: which base do +0x51 and +0x52 belong to?
print("\n=== attribution for the contested fields ===")
for want in (0x51, 0x52, 0x53, 0x54, 0x18, 0x28, 0x5e, 0x66):
    owners = [(f"0x{g:04x}", c[want]) for g, c in per_global.items() if c.get(want)]
    owners.sort(key=lambda t: -t[1])
    print(f"  +0x{want:02x}: " + (", ".join(f"{g}x{c}" for g, c in owners) or "none")
          + (f"  [unattributed={unknown[want]}]" if unknown.get(want) else ""))
