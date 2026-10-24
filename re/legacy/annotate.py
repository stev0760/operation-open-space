#!/usr/bin/env python3
"""
Build the complete annotated API call-site map for INSPACE.exe.

Model (verified against the loader source and the raw bytes):

  * OptLoader relocation records give (segment, fixup offset, module, ordinal)
    for imports, and (segment, fixup offset, selector index) for internal
    cross-segment references.
  * RELOC_INT_PTR16 / RELOC_INT_OFF16 implement CHAINED fixups: the loader
    writes the resolved value at the record's offset, then treats the word
    that WAS there as the offset of the next site in the same chain, repeating
    until that word is 0xFFFF. One record therefore covers several sites.
    Verified: chain links are 100% inside the segment and always point at the
    operand word of another fixup site; all chains terminate at 0xFFFF.

Site geometry / forms:
  far_call   9a <off:2> <seg:2>   import PTR16 -> opcode = offset-1
                                  internal SEG16 -> opcode = offset-3
  mov_imm16  c7 4x <disp> <imm:2> data-slot fixup (far pointer stored in a
                                  data/bss slot) -> opcode = offset-3, and only
                                  ONE word is written, so the 'segment' half
                                  lives in an adjacent slot.

Output: api-sites.json, api-sites.tsv (Ghidra-ready)
"""
import json
import os
import struct
from collections import Counter, defaultdict

PROJ = '/home/cabal/Git/repos/innerspace-rea'
CAP = 4096

FORMS = {0x9A: 'far_call', 0xC7: 'mov_imm16'}


def main():
    rel = json.load(open(os.path.join(PROJ, 'relocs.json')))
    api = json.load(open(os.path.join(PROJ, 'api-imports.json')))
    names = {(r['module'], r['ordinal']): r['name'] for r in api['imports']}

    d = open(os.path.join(PROJ, 'extracted/shipdisk/INSPACE.exe'), 'rb').read()
    ne = struct.unpack_from('<H', d, 0x3c)[0]
    segtab = struct.unpack_from('<H', d, ne + 0x22)[0]
    cseg = struct.unpack_from('<H', d, ne + 0x1c)[0]
    img = {n: open(os.path.join(PROJ, 'unpacked', 'seg%02d.bin' % n), 'rb').read()
           for n in range(1, cseg + 1)}

    sites = []
    stats = Counter()
    unknown_ops = Counter()
    chain_hist = Counter()

    for r in rel['relocs']:
        if r['kind'] == 'import_ordinal' and r['target_type'] == 'PTR16':
            delta, is_imp = 1, True
        elif r['kind'] == 'special_base':
            delta, is_imp = 3, False
        else:
            continue

        seg = r['seg']
        buf = img[seg]
        if is_imp:
            label = names.get((r.get('module'), r['ordinal']),
                              '%s.%d' % (r.get('module'), r['ordinal']))
        else:
            label = 'far_selector_ref'

        seen = set()
        cur = r['offset']
        n = 0
        while True:
            opc = cur - delta
            if cur in seen or opc < 0 or cur + 2 > len(buf):
                stats['walk_fail'] += 1
                break
            seen.add(cur)
            n += 1
            op = buf[opc]
            form = FORMS.get(op)
            if form is None:
                stats['unknown_opcode'] += 1
                unknown_ops['%02x' % op] += 1
                break
            sites.append({
                'seg': seg,
                'selector': 0x1000 + (seg - 1) * 8,
                'ghidra_addr': '%04x:%04x' % (0x1000 + (seg - 1) * 8, opc),
                'opcode_offset': opc,
                'fixup_offset': cur,
                'opcode_byte': op,
                'form': form,
                'api': label,
                'module': r.get('module') if is_imp else None,
                'ordinal': r.get('ordinal') if is_imp else None,
                'kind': 'import' if is_imp else 'internal',
            })
            nxt = struct.unpack_from('<H', buf, cur)[0]
            if nxt == 0xFFFF:
                break
            cur = nxt
            if len(seen) >= CAP:
                stats['capped'] += 1
                break
        chain_hist[n] += 1
        stats['records'] += 1

    print('=== CHAIN WALK (cap=%d) ===' % CAP)
    print('  records walked         : %d' % stats['records'])
    print('  sites recovered        : %d' % len(sites))
    print('  walk failures          : %d' % stats['walk_fail'])
    print('  unknown opcodes        : %d %s'
          % (stats['unknown_opcode'], dict(unknown_ops)))
    print('  capped (should be 0)   : %d' % stats['capped'])
    print('  longest chain          : %d' % max(chain_hist) if chain_hist else 0)

    imp = [s for s in sites if s['kind'] == 'import']
    intern = [s for s in sites if s['kind'] == 'internal']
    print('\n  import sites           : %d' % len(imp))
    print('  internal sites         : %d' % len(intern))
    print('  forms (import)         : %s'
          % dict(Counter(s['form'] for s in imp)))
    print('  forms (internal)       : %s'
          % dict(Counter(s['form'] for s in intern)))

    by_api = defaultdict(list)
    for s in imp:
        by_api[(s['module'], s['ordinal'])].append(s['ghidra_addr'])
    print('  distinct APIs          : %d' % len(by_api))

    json.dump({'sites': sites,
               'totals': {'records': stats['records'],
                          'sites': len(sites),
                          'import_sites': len(imp),
                          'internal_sites': len(intern),
                          'walk_failures': stats['walk_fail'],
                          'distinct_apis': len(by_api)}},
              open(os.path.join(PROJ, 'api-sites.json'), 'w'), indent=1)

    with open(os.path.join(PROJ, 'api-sites.tsv'), 'w') as f:
        f.write('#ghidra_addr\tapi\tmodule\tordinal\tform\n')
        for s in sorted(sites, key=lambda x: (x['seg'], x['opcode_offset'])):
            f.write('%s\t%s\t%s\t%s\t%s\n'
                    % (s['ghidra_addr'], s['api'],
                       s['module'] or '-', s['ordinal'] if s['ordinal'] is not None else '-',
                       s['form']))
    print('\nwrote api-sites.json, api-sites.tsv')


if __name__ == '__main__':
    main()
