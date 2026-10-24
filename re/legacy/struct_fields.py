#!/usr/bin/env python3
"""
Structure field extraction with the stride bound at the array-base load.

Why the earlier attempt failed
------------------------------
Two criteria were tried and both are wrong:
  1. "stride = value of the most recent imul into the index register, within a
     few instructions" - leaks badly, because index registers are reused all over
     a function.
  2. "stride = the candidate minimising distinct(displacement mod stride)" - this
     is DEGENERATE: a small stride bounds the residual set by its own size, so
     S=0x14 always beats S=0x15a regardless of truth.

What actually holds
-------------------
The real code shape is always:
        imul $stride,<src>,%idx      ; build the element offset  (BEFORE the les)
        ...
        les  <global>,%base          ; load the array base pointer
        ... %es:disp(%idx[,%base])   ; field access
So the stride must be captured AT THE `les` from the immediately preceding imul,
and then applied to every access using that base. That binds stride to array base
one-to-one, which is exactly the structure the compiler emitted.

Outputs: struct-fields.json, struct-fields.txt
"""
import json
import os
import re
from collections import Counter, defaultdict

PROJ = '/home/cabal/Git/repos/innerspace-rea'
ASM = os.path.join(PROJ, 'annotated-asm')

LES_RE = re.compile(r'^les\s+(?:0x)?([0-9a-fA-F]+),%([a-z]{2})$')
IMUL_RE = re.compile(r'^imul\s+\$0x([0-9a-fA-F]+),.*,%([a-z]{2})$')
FIELD_RE = re.compile(r'%es:(?:0x([0-9a-fA-F]+))?\(%([a-z]{2})(?:,%([a-z]{2}))?\)')
ES_CLOBBER_RE = re.compile(r'^les\s|^pop\s+%es$|,%es$|^mov\s+%[a-z]{2},%es$')
LINE_RE = re.compile(r'^\s*[0-9a-fA-F]+:\t')

IMUL_LOOKBACK = 16     # the stride imul sits just before the les
BASE_WINDOW = 120      # accesses after the les


def width_of(mnem):
    if mnem.endswith('b'):
        return 1
    if mnem.endswith('l'):
        return 4
    return 2


def main():
    # global -> stride -> field -> Counter(width)
    acc = defaultdict(lambda: defaultdict(lambda: defaultdict(Counter)))
    base = {}          # reg -> (global, ln, stride)
    recent_imul = []   # (ln, stride)

    ln = 0
    for fn in sorted(os.listdir(ASM)):
        if not fn.endswith('.asm'):
            continue
        for line in open(os.path.join(ASM, fn)).read().splitlines():
            if not LINE_RE.match(line):
                continue
            ln += 1
            body = line.split('\t')[-1].strip()
            if ';' in body:
                body = body.split(';')[0].strip()
            if not body:
                continue

            im = IMUL_RE.match(body)
            if im:
                recent_imul.append((ln, int(im.group(1), 16)))
                recent_imul = [x for x in recent_imul if ln - x[0] <= IMUL_LOOKBACK]
                continue

            lm = LES_RE.match(body)
            if lm:
                stride = None
                for l2, s in reversed(recent_imul):
                    if ln - l2 <= IMUL_LOOKBACK:
                        stride = s
                        break
                base = {lm.group(2): (lm.group(1).lower(), ln, stride)}
                recent_imul = []
                continue

            if ES_CLOBBER_RE.search(body):
                base = {}
                continue

            fm = FIELD_RE.search(body)
            if not fm:
                continue
            disp = int(fm.group(1), 16) if fm.group(1) else 0
            r1, r2 = fm.group(2), fm.group(3)
            hit = None
            for r in ([r1] + ([r2] if r2 else [])):
                if r in base and ln - base[r][1] <= BASE_WINDOW:
                    hit = base[r]
                    break
            if hit is None:
                continue
            g, _, stride = hit
            key = stride if stride else 'unknown'
            off = disp % stride if stride else disp
            acc[g][key][off][width_of(body.split()[0])] += 1

    out = {}
    for g, strides in acc.items():
        total = sum(c for st in strides.values() for f in st.values() for c in f.values())
        if total < 8:
            continue
        out[g] = {}
        for st, fields in strides.items():
            out[g][str(st) if st != 'unknown' else 'unknown'] = {
                'accesses': sum(c for f in fields.values() for c in f.values()),
                'field_count': len(fields),
                'fields': {str(o): dict(c) for o, c in sorted(fields.items())},
            }
    json.dump(out, open(os.path.join(PROJ, 'struct-fields.json'), 'w'), indent=1)

    with open(os.path.join(PROJ, 'struct-fields.txt'), 'w') as f:
        f.write('STRUCT FIELDS (stride bound at the array-base `les`)\n')
        f.write('=' * 74 + '\n')
        for g, strides in sorted(out.items(),
                                 key=lambda kv: -sum(d['accesses'] for d in kv[1].values())):
            f.write('\nDGROUP:0x%s\n' % g)
            for st, d in sorted(strides.items(), key=lambda kv: -kv[1]['accesses']):
                if st == 'unknown':
                    f.write('  [stride unresolved] accesses=%d fields=%d\n'
                            % (d['accesses'], d['field_count']))
                    continue
                f.write('  stride 0x%x (%d bytes), accesses=%d, fields=%d\n'
                        % (int(st), int(st), d['accesses'], d['field_count']))
                for o in sorted(int(k) for k in d['fields']):
                    f.write('      +0x%04x  widths=%s\n' % (o, d['fields'][str(o)]))

    print('=== STRUCT FIELDS (stride bound at les) ===')
    shown = 0
    for g, strides in sorted(out.items(),
                             key=lambda kv: -sum(d['accesses'] for d in kv[1].values())):
        real = {st: d for st, d in strides.items() if st != 'unknown'}
        if not real:
            continue
        st = max(real, key=lambda x: real[x]['accesses'])
        d = real[st]
        print('  DGROUP:0x%-6s stride 0x%-5x = %4d bytes  accesses=%-5d fields=%d'
              % (g, int(st), int(st), d['accesses'], d['field_count']))
        print('      %s' % ', '.join('0x%x' % o for o in sorted(int(k) for k in d['fields'])))
        shown += 1
        if shown >= 12:
            break
    print('\nwrote struct-fields.json, struct-fields.txt')


if __name__ == '__main__':
    main()
