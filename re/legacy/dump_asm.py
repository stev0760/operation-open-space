#!/usr/bin/env python3
"""
Print a range (or a window around an address) of the annotated disassembly.

Usage:
  python3 dump_asm.py <addr> [before] [after]      # window around a Ghidra addr
  python3 dump_asm.py --seg6 0x43e0 0x45b0         # explicit segment/range
  python3 dump_asm.py --fun <addr>                 # from addr to the next ret/prologue

Addr format: SSSS:OOOO (selector:offset) or a bare offset for --segN mode.
"""
import os
import re
import sys

PROJ = '/home/cabal/Git/repos/innerspace-rea'
ASM = os.path.join(PROJ, 'annotated-asm')
LINE_RE = re.compile(r'^\s*([0-9a-fA-F]+):\t')


def load(seg):
    path = os.path.join(ASM, 'seg%02d.asm' % seg)
    rows = []
    for line in open(path).read().splitlines():
        m = LINE_RE.match(line)
        if m:
            rows.append((int(m.group(1), 16), line))
    return rows


def sel_to_seg(sel):
    return (sel - 0x1000) // 8 + 1


def main():
    args = sys.argv[1:]

    if args and args[0].startswith('--seg'):
        seg = int(args[0][5:])
        lo = int(args[1], 16)
        hi = int(args[2], 16)
    elif args and args[0] == '--fun':
        sel, off = args[1].split(':')
        seg = sel_to_seg(int(sel, 16))
        lo = int(off, 16)
        rows = load(seg)
        hi = None
        for a, line in rows:
            if a <= lo:
                continue
            body = line.split('\t')[-1]
            if 'ret' in body.split(';')[0] and a > lo:
                hi = a + 8
                break
        if hi is None:
            hi = lo + 0x400
    else:
        sel, off = args[0].split(':')
        seg = sel_to_seg(int(sel, 16))
        lo = int(off, 16) - int(args[1], 16) if len(args) > 1 else 0
        hi = int(off, 16) + (int(args[2], 16) if len(args) > 2 else 0x120)

    rows = load(seg)
    print('; segment %d (selector 0x%04x) range 0x%04x..0x%04x'
          % (seg, 0x1000 + (seg - 1) * 8, lo, hi))
    for a, line in rows:
        if lo <= a <= hi:
            # annotate with the Ghidra address for reference
            parts = line.split('\t')
            print('%04x:%04x  %s' % (0x1000 + (seg - 1) * 8, a,
                                     '\t'.join(parts[1:]).replace('\t\t', '\t')))


if __name__ == '__main__':
    main()
