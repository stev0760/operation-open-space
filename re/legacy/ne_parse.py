#!/usr/bin/env python3
"""NE (New Executable) parser v4 — winedump layout (ne_crc is LONG).
+00 'NE' | +02 ver B | +03 rev B | +04 enttab W | +06 cbenttab W
+08 crc LONG | +0c flags W | +0e autodata W | +10 heap W | +12 stack W
+14 csip DWORD | +18 sssp DWORD | +1c cseg W | +1e cmod W | +20 cbnrestab W
+22 segtab W | +24 rsrctab W | +26 restab W | +28 modtab W | +2a imptab W
+2c nrestab DWORD (file off) | +30 cmovent W | +32 align W | +34 cres W
+36 exetyp B | +37 flagsothers B
Verified against INSPACE.exe: cmod=5 (matches r2 modules), autodata=19,
sssp=0013:0000, exetyp=2 (Windows), tables ordered consistently.
"""
import sys, json, struct

RES_TYPES = {1:'CURSOR',2:'BITMAP',3:'ICON',4:'MENU',5:'DIALOG',6:'STRING',
             7:'FONTDIR',8:'FONT',9:'ACCEL',10:'RCDATA',11:'GROUP_CURSOR',
             12:'GROUP_ICON',14:'GROUP_ICON',16:'VERSION',17:'DLGINCLUDE',
             19:'PLUGPLAY',20:'VXD',21:'ANICURSOR',22:'ANIICON'}

def parse_ne(path):
    d = open(path, 'rb').read()
    h = struct.unpack_from('<I', d, 0x3c)[0]
    assert d[h:h+2] == b'NE', f"no NE at {h:#x}"
    g = lambda o: struct.unpack_from('<H', d, h + o)[0]
    gd = lambda o: struct.unpack_from('<I', d, h + o)[0]
    out = {'file': path, 'ne_off': hex(h),
           'linker_ver': f"{d[h+2]}.{d[h+3]}",
           'enttab': g(4), 'cbenttab': g(6), 'crc': gd(8),
           'flags': hex(g(0x0c)),
           'autodata_seg': g(0x0e), 'heap': g(0x10), 'stack': g(0x12),
           'csip': {'cs': gd(0x14) >> 16, 'ip': gd(0x14) & 0xffff},
           'sssp': {'ss': gd(0x18) >> 16, 'sp': gd(0x18) & 0xffff},
           'cseg': g(0x1c), 'cmod': g(0x1e),
           'cbnrestab': g(0x20),
           'segtab': g(0x22), 'rsrctab': g(0x24), 'restab': g(0x26),
           'modtab': g(0x28), 'imptab': g(0x2a),
           'nrestab': gd(0x2c),
           'cmovent': g(0x30), 'align_shift': g(0x32), 'cres': g(0x34),
           'exetyp': d[h+0x36], 'target_os': {1:'OS/2',2:'Windows',3:'Euro MS-DOS 4',4:'Windows/386',5:'BOSS'}.get(d[h+0x36], d[h+0x36]),
           'flagsothers': d[h+0x37]}
    segtab, rsrctab, restab, modtab, imptab = out['segtab'], out['rsrctab'], out['restab'], out['modtab'], out['imptab']
    alignshift, cseg, cmod = out['align_shift'], out['cseg'], out['cmod']
    # Segment table: cseg × {offset W (align units), length W, flags W, minalloc W}
    segs = []
    for i in range(cseg):
        off, ln, flags, minalloc = struct.unpack_from('<HHHH', d, h + segtab + i*8)
        segs.append({'n': i+1, 'file_off': off << alignshift if off else 0,
                     'len': ln, 'flags': hex(flags), 'minalloc': minalloc,
                     'code': bool(flags & 0x0001 or not (flags & 0x0002)),
                     'data': bool(flags & 0x0002),
                     'movable': bool(flags & 0x0010),
                     'preload': not (flags & 0x0020),
                     'discardable': bool(flags & 0xf000)})
    out['segments'] = segs
    # Resident names (rel to NE hdr): first = module name, rest = exported ordinals
    names = []
    p = h + restab
    while True:
        ln = d[p]
        if ln == 0: break
        nm = d[p+1:p+1+ln].decode('latin1')
        ordinal = struct.unpack_from('<H', d, p+1+ln)[0]
        names.append({'name': nm, 'ordinal': ordinal})
        p += 1 + ln + 2
    out['resident_names'] = names
    # Non-resident names (FILE offset, length cbnrestab)
    nnames = []
    if out['nrestab']:
        p = out['nrestab']
        end = p + out['cbnrestab']
        while p < end and p < len(d):
            ln = d[p]
            if ln == 0: break
            nm = d[p+1:p+1+ln].decode('latin1')
            ordinal = struct.unpack_from('<H', d, p+1+ln)[0]
            nnames.append({'name': nm, 'ordinal': ordinal})
            p += 1 + ln + 2
    out['nonresident_names'] = nnames
    # Module refs (rel to NE hdr): WORDs → imported names offsets (rel imptab)
    mods = []
    for i in range(cmod):
        noff = struct.unpack_from('<H', d, h + modtab + i*2)[0]
        mn_len = d[h + imptab + noff]
        mn = d[h + imptab + noff + 1: h + imptab + noff + 1 + mn_len].decode('latin1')
        mods.append(mn)
    out['modules'] = mods
    # Entry table (rel to NE hdr, cbenttab bytes): bundles
    ents = []
    p = h + out['enttab']
    end = p + out['cbenttab']
    while p < end:
        cnt = d[p]
        if cnt == 0: break
        segnum = d[p+1]
        p += 2
        for _ in range(cnt):
            if segnum == 0: break
            if segnum == 0xff:  # movable: flags B, int W, off W
                flags3, intval, offval = struct.unpack_from('<BHH', d, p)
                ents.append({'seg': segnum, 'flags': flags3, 'int': intval, 'off': offval})
                p += 5
            else:  # fixed: flags B, off W
                flags3, offval = struct.unpack_from('<BH', d, p)
                ents.append({'seg': segnum, 'flags': flags3, 'off': offval})
                p += 3
    out['entry_table'] = ents
    # Resource table (rel to NE hdr): align W then type entries
    reses = []
    p = h + rsrctab
    shift = struct.unpack_from('<H', d, p)[0]
    p += 2
    while True:
        if p + 4 > len(d): break
        rtype, count = struct.unpack_from('<HH', d, p)
        if rtype == 0: break
        tname = RES_TYPES.get(rtype & 0x7fff, hex(rtype))
        for j in range(count):
            if p + 4 + j*12 + 12 > len(d): break
            roff, rlen, f1, f2, f3, fid = struct.unpack_from('<HHHHHH', d, p+4+j*12)
            rname = None
            if fid & 0x8000:
                no = h + rsrctab + (fid & 0x7fff)
                nlen = d[no]
                rname = d[no+1:no+1+nlen].decode('latin1')
            reses.append({'type': tname, 'id': rname if rname else fid,
                          'file_off': roff << shift, 'len': rlen})
        p += 4 + count * 12
    out['resources'] = reses
    return out

if __name__ == '__main__':
    print(json.dumps(parse_ne(sys.argv[1]), indent=1))