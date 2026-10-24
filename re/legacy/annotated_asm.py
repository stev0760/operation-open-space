#!/usr/bin/env python3
"""
Produce the annotated disassembly for every code segment of INSPACE.exe.

Independent of Ghidra: objdump the decompressed segment image with
--adjust-vma=0 so instruction addresses equal the intra-segment offsets used by
api-sites.json, then tag every fixup site with its resolved API.

Outputs:
  annotated-asm/segNN.asm   full listing with API tags appended
  api-calls.txt             one line per call site, address order
  api-calls-by-seg.txt      grouped per segment
"""
import json
import os
import re
import struct
import subprocess
sys_path = '/home/cabal/Git/repos/innerspace-rea'

PROJ = '/home/cabal/Git/repos/innerspace-rea'
OUT = os.path.join(PROJ, 'annotated-asm')

LINE_RE = re.compile(r'^\s*([0-9a-fA-F]+):\t(.*)$')


def main():
    sites = json.load(open(os.path.join(PROJ, 'api-sites.json')))['sites']
    by_seg = {}
    for s in sites:
        by_seg.setdefault(s['seg'], {})[s['opcode_offset']] = s

    d = open(os.path.join(PROJ, 'extracted/shipdisk/INSPACE.exe'), 'rb').read()
    ne = struct.unpack_from('<H', d, 0x3c)[0]
    segtab = struct.unpack_from('<H', d, ne + 0x22)[0]
    cseg = struct.unpack_from('<H', d, ne + 0x1c)[0]

    os.makedirs(OUT, exist_ok=True)
    all_calls = []
    per_seg_counts = {}

    for i in range(cseg):
        seg = i + 1
        fo, ln, fl, ma = struct.unpack_from('<HHHH', d, ne + segtab + i * 8)
        if fl & 1:
            continue     # skip data segments
        path = os.path.join(PROJ, 'unpacked', 'seg%02d.bin' % seg)
        if not os.path.exists(path):
            continue
        p = subprocess.run(['objdump', '-D', '-b', 'binary', '-m', 'i8086',
                            '--adjust-vma=0', path],
                           capture_output=True, text=True)
        if p.returncode != 0:
            print('  seg%d objdump failed: %s' % (seg, p.stderr[:120]))
            continue

        sel = 0x1000 + (seg - 1) * 8
        smap = by_seg.get(seg, {})
        out_lines = []
        hits = 0
        for line in p.stdout.splitlines():
            m = LINE_RE.match(line)
            if m:
                addr = int(m.group(1), 16)
                s = smap.get(addr)
                if s:
                    hits += 1
                    if s['kind'] == 'import':
                        tag = '  ; ==> %s!%s  [%s.%d]' % (
                            s['module'], s['api'], s['module'], s['ordinal'])
                    else:
                        tag = '  ; ==> internal cross-segment call'
                    line = line + tag
                    all_calls.append('%04x:%04x\t%s\t%s\t%d\t%s' % (
                        sel, addr, s['api'],
                        s['module'] or '-',
                        s['ordinal'] if s['ordinal'] is not None else 0,
                        s['form']))
            out_lines.append(line)

        with open(os.path.join(OUT, 'seg%02d.asm' % seg), 'w') as f:
            f.write('; annotated disassembly of segment %d '
                    '(file_off=0x%04x len=%d minalloc=%d)\n' % (seg, fo, ln, ma))
            f.write('; selector 0x%04x; %d of %d fixup sites matched to an '
                    'instruction\n' % (sel, hits, len(smap)))
            f.write('\n'.join(out_lines))
            f.write('\n')
        per_seg_counts[seg] = (hits, len(smap))

    all_calls.sort()
    with open(os.path.join(PROJ, 'api-calls.txt'), 'w') as f:
        f.write('#ghidra_addr\tapi\tmodule\tordinal\tform\n')
        for c in all_calls:
            f.write(c + '\n')

    with open(os.path.join(PROJ, 'api-calls-by-seg.txt'), 'w') as f:
        f.write('segment  matched/total fixup sites\n')
        for seg in sorted(per_seg_counts):
            h, t = per_seg_counts[seg]
            f.write('  seg%-3d  %5d / %5d\n' % (seg, h, t))

    print('=== ANNOTATED DISASSEMBLY ===')
    for seg in sorted(per_seg_counts):
        h, t = per_seg_counts[seg]
        print('  seg%-3d  sites matched to instructions: %5d / %5d' % (seg, h, t))
    print('  total call sites written to api-calls.txt: %d' % len(all_calls))


if __name__ == '__main__':
    main()
