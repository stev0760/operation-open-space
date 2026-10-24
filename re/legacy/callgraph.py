#!/usr/bin/env python3
"""
Build an internal function-entry inventory and call graph.

Only the HEAD of a special_base chain carries a true target offset: for the other
sites in the chain the offset word is a link to the next site. So the head of each
record gives (target_seg, target_off), i.e. a function entry, and the site that
contains it is the caller. That yields an internal call graph without relying on
Ghidra's damaged NE function inventory.

Usage: callgraph.py
"""
import json
import os
import struct
from collections import defaultdict

WS = '/home/cabal/Git/repos/innerspace-rea'
UNP = os.path.join(WS, 'unpacked')

rel = json.load(open(os.path.join(WS, 'relocs.json')))
sites = json.load(open(os.path.join(WS, 'api-sites.json')))['sites']

segs = {}
for n in range(1, 20):
    p = os.path.join(UNP, 'seg%02d.bin' % n)
    if os.path.exists(p):
        segs[n] = open(p, 'rb').read()

# site lookup by (seg, opcode_offset) so we can tell heads from chain members
head_offsets = set()
for r in rel['relocs']:
    if r['kind'] == 'special_base':
        head_offsets.add((r['seg'], r['offset'] - 3))

entries = {}          # (tseg, toff) -> set of caller (seg, off)
for r in rel['relocs']:
    if r['kind'] != 'special_base':
        continue
    seg, o = r['seg'], r['offset']
    b = segs.get(seg, b'')
    if o - 3 < 0 or o - 3 >= len(b) or b[o - 3] != 0x9a:
        continue                                   # the c7 data-slot records
    toff = struct.unpack_from('<H', b, o - 2)[0]
    tseg = r['selector_idx']
    entries.setdefault((tseg, toff), set()).add((seg, o - 3))

print('=== internal function entries (head of each special_base record) ===')
print('distinct entries: %d' % len(entries))

by_seg = defaultdict(list)
for (tseg, toff), callers in entries.items():
    by_seg[tseg].append((toff, len(callers)))
print('\nentries per target segment:',
      {k: len(v) for k, v in sorted(by_seg.items())})

print('\n=== most-called internal functions (ranked) ===')
ranked = sorted(entries.items(), key=lambda kv: -len(kv[1]))
for (tseg, toff), callers in ranked[:25]:
    cl = sorted(callers)[:4]
    print('  seg%-2d:0x%04x  callers=%d   e.g. %s'
          % (tseg, toff, len(callers),
             ' '.join('%d:0x%04x' % (s, o) for s, o in cl)))

print('\n=== entries in seg4 (target segment for our accumulator work) ===')
for toff, n in sorted(by_seg.get(4, [])):
    print('  seg4:0x%04x  called by %d site(s)' % (toff, n))

with open(os.path.join(WS, 'callgraph.json'), 'w') as f:
    json.dump({'entries': [{'tseg': k[0], 'toff': k[1],
                            'callers': sorted(list(v))}
                           for k, v in entries.items()]}, f, indent=1)
print('\nwrote callgraph.json')
