#!/usr/bin/env python3
"""Cross-validate the prologue-scan inventory against same-segment NEAR CALL
targets -- a completely independent signal (the call graph and relocations cannot
see near calls at all).

Also: do the segments that contain `call 0x0` have a prologue-scan entry at offset 0?
"""
import json, glob, os, re, collections

ent = {int(k): sorted(v) for k, v in json.load(open("function-entries.json")).items()}

CALL = re.compile(r'[ \t]call[ \t]+0x([0-9a-f]+)')
LINE = re.compile(r'^\s*([0-9a-f]+):')

print("=== does `call 0x0` land on a prologue-scan entry? (segments that contain it) ===")
for seg in (3, 15, 16, 17, 4):
    hits = [0 in ent.get(seg, [])]
    print(f"  seg{seg:<3} entry at offset 0? {'YES' if hits[0] else 'NO '}   "
          f"(entries in seg: {len(ent.get(seg, []))})")

print("\n=== near CALL targets vs prologue-scan entries, per segment ===")
tot_targets = tot_matched = 0
for path in sorted(glob.glob("annotated-asm/seg*.asm")):
    seg = int(''.join(c for c in os.path.basename(path) if c.isdigit()))
    targets = set()
    for line in open(path, encoding="utf-8", errors="replace"):
        m = CALL.search(line)
        if m:
            targets.add(int(m.group(1), 16))
    if not targets:
        continue
    entries = set(ent.get(seg, []))
    matched = targets & entries
    missing = sorted(targets - entries)
    tot_targets += len(targets); tot_matched += len(matched)
    print(f"  seg{seg:<3} near-call targets={len(targets):>3}  in inventory={len(matched):>3}"
          f"  NOT in inventory={len(missing)}"
          + (f"  {[hex(x) for x in missing[:6]]}" if missing else ""))

print(f"\nTOTAL near-call targets {tot_targets}, matched to inventory {tot_matched} "
      f"({tot_matched/max(tot_targets,1)*100:.1f}%)")

# the near-call targets give a NEW lower bound on real functions
allt = 0
for path in sorted(glob.glob("annotated-asm/seg*.asm")):
    seg = int(''.join(c for c in os.path.basename(path) if c.isdigit()))
    for line in open(path, encoding="utf-8", errors="replace"):
        if CALL.search(line):
            allt += 1
print(f"near-call instruction count: {allt}")
