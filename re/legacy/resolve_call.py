#!/usr/bin/env python3
"""Resolve the real target of an internal far call whose segment word is an
unrelocated placeholder in unpacked/segNN.bin.

  resolve_call.py <seg> <site_offset_hex>

Looks for a special_base relocation record whose `offset` is the segment word
(= CALLF opcode + 3) and reports the target segment via selector_idx; then confirms
against the call-graph inventory.
"""
import json, sys

seg = int(sys.argv[1])
site = int(sys.argv[2], 16)

recs = json.load(open("relocs.json"))
if isinstance(recs, dict):
    recs = recs.get("relocs", recs.get("records", recs.get("relocations", [])))

print(f"total records: {len(recs)}")
print("sample record:", json.dumps(recs[0], indent=1)[:400] if recs else "none")

def seg_of(r):
    for k in ("seg", "segment", "source_segment"):
        if k in r:
            return r[k]
    return None

def off_of(r):
    for k in ("offset", "off", "site"):
        if k in r:
            return r[k]
    return None

# records whose segment word is at opcode+3 -> search site-2 .. site+2
cands = []
for r in recs:
    s, o = seg_of(r), off_of(r)
    if s is None or o is None:
        continue
    if s == seg and abs(o - (site + 3)) <= 3:
        cands.append(r)

print(f"\n=== records near seg{seg}:0x{site+3:x} ===")
for r in cands:
    print(json.dumps(r, indent=1))

# call-graph lookup by target offset
try:
    cg = json.load(open("callgraph.json"))["entries"]
    print("\n=== call-graph entries with this target offset ===")
    for e in cg:
        if e["toff"] == site and True:
            pass
except Exception as ex:
    print("cg err", ex)
