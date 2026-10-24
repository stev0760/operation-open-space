#!/usr/bin/env python3
"""
Validate the decoded relocation records against the raw segment bytes, and
tabulate the distinct (module, ordinal) import set.

For an import-by-ordinal PTR16 fixup, PROCESS_RELOCS writes:
    ES:[DI]   = import offset   (DI = record 'offset')
    ES:[DI+2] = import segment
The unpatched placeholder therefore is  lcall 0000:ffff  == bytes
    9a  ff ff  00 00
at (offset-1), i.e. the 0x9a opcode sits at offset-1.
"""
import json
import os
import struct
import sys
from collections import Counter, defaultdict

PROJ = '/home/cabal/Git/repos/innerspace-rea'
EXE = os.path.join(PROJ, 'extracted/shipdisk/INSPACE.exe')


def main():
    d = open(EXE, 'rb').read()
    ne = struct.unpack_from('<H', d, 0x3c)[0]
    segtab = struct.unpack_from('<H', d, ne + 0x22)[0]
    align = struct.unpack_from('<H', d, ne + 0x32)[0]
    cseg = struct.unpack_from('<H', d, ne + 0x1c)[0]
    segs = {}
    for i in range(cseg):
        off, ln, fl, ma = struct.unpack_from('<HHHH', d, ne + segtab + i * 8)
        segs[i + 1] = {'file_off': off << align, 'len': ln, 'minalloc': ma}

    # use the unpacked (decompressed) images so offsets are in image space
    img = {}
    for n in range(1, cseg + 1):
        p = os.path.join(PROJ, 'unpacked', 'seg%02d.bin' % n)
        img[n] = open(p, 'rb').read()

    j = json.load(open(os.path.join(PROJ, 'relocs.json')))
    rel = j['relocs']

    ok = bad = 0
    badlist = []
    for r in rel:
        n = r['seg']
        buf = img[n]
        o = r['offset']
        if r['kind'] == 'import_ordinal' and r['target_type'] == 'PTR16':
            if o - 1 >= 0 and buf[o - 1] == 0x9A and \
               struct.unpack_from('<HH', buf, o) == (0xFFFF, 0x0000):
                ok += 1
            else:
                bad += 1
                if len(badlist) < 12:
                    badlist.append(('import', n, hex(o),
                                    buf[o - 1:o + 3].hex()))
        elif r['kind'] == 'special_base':
            if o - 1 >= 0 and buf[o - 1] == 0x9A and \
               struct.unpack_from('<H', buf, o + 2)[0] == 0xFFFF:
                ok += 1
            else:
                bad += 1
                if len(badlist) < 12:
                    badlist.append(('special', n, hex(o),
                                    buf[o - 1:o + 3].hex()))

    print('SITE VALIDATION')
    print('  confirmed placeholder sites: %d' % ok)
    print('  mismatched:                  %d' % bad)
    for b in badlist:
        print('    %-8s seg%-3d off=%-8s bytes=%s' % b)

    # distinct imports
    per_mod = defaultdict(Counter)
    for r in rel:
        if r['kind'] == 'import_ordinal':
            per_mod[r.get('module', '?')][r['ordinal']] += 1
    print('\nDISTINCT IMPORTS BY MODULE')
    total = 0
    for m in sorted(per_mod):
        c = per_mod[m]
        total += len(c)
        print('  %-9s ordinals=%-4d call sites=%d' % (m, len(c), sum(c.values())))
        print('     %s' % ' '.join(str(x) for x in sorted(c)))
    print('  TOTAL distinct imported ordinals: %d' % total)

    # most-used
    print('\nMOST-USED IMPORTS (module, ordinal) -> call sites')
    allc = Counter()
    for r in rel:
        if r['kind'] == 'import_ordinal':
            allc[(r.get('module'), r['ordinal'])] += 1
    for (m, o), n in allc.most_common(25):
        print('  %-9s .%-5d  x%d' % (m, o, n))

    out = {'per_module': {m: dict(c) for m, c in per_mod.items()},
           'site_counts': {'ok': ok, 'bad': bad}}
    json.dump(out, open(os.path.join(PROJ, 'import-set.json'), 'w'), indent=1)
    print('\nwrote import-set.json')


if __name__ == '__main__':
    main()
