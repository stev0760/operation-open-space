#!/usr/bin/env python3
"""
Find the callers of a given internal (cross-segment) function.

OptLoader's `special_base` relocation patches ONLY the segment word of an
`lcall`, leaving the target offset intact. So a call to target offset 0 of some
segment appears in the unpacked image as `9a 00 00 <seg-placeholder>`, and the
relocation record at that site names which segment it resolves to.

For a special_base record:
    record.offset  = the SEGMENT word site  = opcode + 3
    opcode         = record.offset - 3
    target offset  = word at (record.offset - 2)
    target segment = record.selector_idx  (segment number, 1-based)

Usage: python3 find_callers.py <target_seg> <hex_target_offset>
"""
import json
import os
import struct
import sys
from collections import defaultdict

PROJ = '/home/cabal/Git/repos/innerspace-rea'


def main():
    want_seg = int(sys.argv[1]) if len(sys.argv) > 1 else 4
    want_off = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x0000

    rel = json.load(open(os.path.join(PROJ, 'relocs.json')))
    d = open(os.path.join(PROJ, 'extracted/shipdisk/INSPACE.exe'), 'rb').read()
    ne = struct.unpack_from('<H', d, 0x3c)[0]
    cseg = struct.unpack_from('<H', d, ne + 0x1c)[0]
    img = {n: open(os.path.join(PROJ, 'unpacked', 'seg%02d.bin' % n), 'rb').read()
           for n in range(1, cseg + 1)}

    hits = []
    all_zero_calls = defaultdict(int)
    total = 0

    for r in rel['relocs']:
        if r['kind'] != 'special_base':
            continue
        total += 1
        seg = r['seg']
        buf = img[seg]
        off = r['offset']
        opc = off - 3
        if opc < 0 or off + 2 > len(buf):
            continue
        if buf[opc] != 0x9A:
            continue
        tgt_off = struct.unpack_from('<H', buf, off - 2)[0]
        tgt_seg = r['selector_idx']
        if tgt_off == 0:
            all_zero_calls[(tgt_seg, seg)] += 1
        if tgt_seg == want_seg and tgt_off == want_off:
            hits.append((seg, opc, tgt_seg, tgt_off))

    print('special_base records examined: %d' % total)
    print('\n=== callers of seg%d:0x%04x ===' % (want_seg, want_off))
    if not hits:
        print('  none found')
    for seg, opc, ts, to in hits:
        print('  caller %04x:%04x  ->  seg%d:0x%04x'
              % (0x1000 + (seg - 1) * 8, opc, ts, to))

    print('\n=== all far calls to offset 0x0000 (callee_seg, caller_seg) ===')
    for (ts, cs), n in sorted(all_zero_calls.items(), key=lambda kv: -kv[1])[:30]:
        print('  target seg%-3d  from caller seg%-3d  x%d' % (ts, cs, n))


if __name__ == '__main__':
    main()
