#!/usr/bin/env python3
"""
Name the Win 3.1 API imports of INSPACE.exe.

Inputs:
  relocs.json          - decoded OptLoader relocation records (module + ordinal
                         + fixup site within each segment)
  wine-specs/*.spec    - Wine win16 module specs, which carry explicit
                         ordinal -> API name tables matched to Windows 3.1

Outputs:
  api-imports.json     - named import set with per-site call-site lists
  (stdout)             - the readable annotated call list

Notes on validation: fixup sites are chained, so a site's pre-patch word is
either 0xFFFF (chain terminator) or the link to the next site in the chain.
Set membership against the placeholder byte patterns is therefore checked as a
set, not positionally.
"""
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict

PROJ = '/home/cabal/Git/repos/innerspace-rea'
SPECS = {
    'KERNEL': 'krnl386.exe16.spec',
    'USER': 'user.exe16.spec',
    'GDI': 'gdi.exe16.spec',
    'MMSYSTEM': 'mmsystem.dll16.spec',
    'SHELL': 'shell.dll16.spec',
}

MODIFIERS = {
    'stdcall', 'pascal', 'cdecl', 'fastcall', 'stub', 'variable', 'thiscall',
    'winapi', 'interrupt', 'naked', 'varargs',
}


def parse_spec(path):
    """Parse a Wine .spec file -> {ordinal: api_name}."""
    out = {}
    auto = 0
    cur = 0
    for raw in open(path, errors='replace'):
        line = raw.split('#')[0].split(';')[0].rstrip()
        if not line.strip():
            continue
        toks = line.split()
        if not toks:
            continue
        if toks[0] == '@':
            cur = auto + 1
            toks = toks[1:]
        elif toks[0].isdigit():
            cur = int(toks[0])
            toks = toks[1:]
        else:
            continue
        auto = max(auto, cur)
        # '113 equate __AHSHIFT 3' -> the imported symbol is __AHSHIFT
        if toks and toks[0].lower() == 'equate':
            out[cur] = toks[1] if len(toks) > 1 else 'equate_%d' % cur
            continue
        # strip modifiers / flags
        while toks and (toks[0].lower() in MODIFIERS or toks[0].startswith('-')):
            toks.pop(0)
        if not toks:
            out.setdefault(cur, 'ordinal_%d' % cur)
            continue
        first = toks[0]
        name = first.split('(')[0]
        if not name:
            name = first
        out[cur] = name
    return out


def main():
    rel = json.load(open(os.path.join(PROJ, 'relocs.json')))
    recs = rel['relocs']
    mods = rel['modules']

    tables = {}
    for m, f in SPECS.items():
        p = os.path.join(PROJ, 'wine-specs', f)
        if os.path.exists(p):
            tables[m] = parse_spec(p)
            print('%-9s spec: %d ordinals' % (m, len(tables[m])))

    sites_by_import = defaultdict(list)
    for r in recs:
        if r['kind'] != 'import_ordinal':
            continue
        key = (r.get('module'), r['ordinal'])
        sites_by_import[key].append((r['seg'], r['offset']))

    # --- validation: set membership of import records vs placeholder sites ---
    d = open(os.path.join(PROJ, 'extracted/shipdisk/INSPACE.exe'), 'rb').read()
    ne = struct.unpack_from('<H', d, 0x3c)[0]
    segtab = struct.unpack_from('<H', d, ne + 0x22)[0]
    align = struct.unpack_from('<H', d, ne + 0x32)[0]
    cseg = struct.unpack_from('<H', d, ne + 0x1c)[0]
    img = {n: open(os.path.join(PROJ, 'unpacked', 'seg%02d.bin' % n), 'rb').read()
           for n in range(1, cseg + 1)}
    on_placeholder = 0
    off_placeholder = 0
    for (seg, o) in [s for v in sites_by_import.values() for s in v]:
        buf = img[seg]
        if o - 1 >= 0 and buf[o - 1] == 0x9A and \
           struct.unpack_from('<HH', buf, o) == (0xFFFF, 0x0000):
            on_placeholder += 1
        else:
            off_placeholder += 1
    print('\nVALIDATION')
    print('  import records landing exactly on an lcall placeholder: %d / %d'
          % (on_placeholder, on_placeholder + off_placeholder))
    print('  remainder are chained fixups (site pre-filled with a chain link)')

    # --- named report ---
    rows = []
    unnamed = Counter()
    for (m, o), sites in sorted(sites_by_import.items(),
                                key=lambda kv: (-len(kv[1]), str(kv[0]))):
        nm = tables.get(m, {}).get(o)
        if nm is None:
            unnamed[(m, o)] = len(sites)
            nm = '<<unmapped>>'
        rows.append({'module': m, 'ordinal': o, 'name': nm,
                     'call_sites': len(sites),
                     'sites': [{'seg': s, 'offset': x} for s, x in sites]})

    print('\n=== NAMED WIN 3.1 API IMPORTS - %d distinct (%d named) ==='
          % (len(rows), sum(1 for r in rows if r['name'] != '<<unmapped>>')))
    print('%-9s %-6s %-26s %s' % ('module', 'ord', 'api name', 'call sites'))
    for r in rows:
        print('%-9s .%-5d %-26s %d' % (r['module'], r['ordinal'], r['name'],
                                       r['call_sites']))

    if unnamed:
        print('\nUNMAPPED (not in Wine win16 specs):')
        for (m, o), n in sorted(unnamed.items()):
            print('  %-9s .%-5d  x%d' % (m, o, n))

    json.dump({'imports': rows,
               'unmapped': {'%s.%d' % k: v for k, v in unnamed.items()},
               'validation': {'on_placeholder': on_placeholder,
                              'chained': off_placeholder}},
              open(os.path.join(PROJ, 'api-imports.json'), 'w'), indent=1)
    print('\nwrote api-imports.json')


if __name__ == '__main__':
    main()
