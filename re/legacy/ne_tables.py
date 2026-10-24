#!/usr/bin/env python3
"""
NE table + relocation parser for INSPACE.exe (and any Win3.1 NE).

Parses, with explicit field offsets and self-validating cursor positions:
  - NE header fields (raw, both NE-relative and absolute file offsets)
  - resident name table
  - module reference table
  - imported names table
  - entry table
  - per-segment relocation records (fixups), including WIN API import fixups

Relocation tables are validated by checking that the record cursor lands
exactly on (or within alignment padding of) the next segment's file offset.

Output: JSON to stdout (or --json file).
"""
import struct
import sys
import json


class NE:
    def __init__(self, path):
        self.path = path
        self.d = open(path, 'rb').read()
        self.ne = struct.unpack_from('<H', self.d, 0x3c)[0]
        assert self.d[self.ne:self.ne + 2] == b'NE', 'not an NE image'

    def w(self, o):
        """word at NE-relative offset o"""
        return struct.unpack_from('<H', self.d, self.ne + o)[0]

    def dw(self, o):
        return struct.unpack_from('<I', self.d, self.ne + o)[0]

    def header(self):
        h = {
            'file': self.path,
            'ne_file_off': self.ne,
            'linker_minor': self.d[self.ne + 2],
            'linker_major': self.d[self.ne + 3],
            'enttab': self.w(0x04),
            'cbenttab': self.w(0x06),
            'crc': self.dw(0x08),
            'flags': self.w(0x0c),
            'autodata_seg': self.w(0x0e),
            'heap': self.w(0x10),
            'stack': self.w(0x12),
            'cs': self.w(0x14), 'ip': self.w(0x16),
            'ss': self.w(0x18), 'sp': self.w(0x1a),
            'cseg': self.w(0x1c),
            'cmod': self.w(0x1e),
            'cbnrestab': self.w(0x20),
            'segtab': self.w(0x22),
            'rsrctab': self.w(0x24),
            'restab': self.w(0x26),
            'modtab': self.w(0x28),
            'imptab': self.w(0x2a),
            'nrestab_file': self.dw(0x2c),
            'cmovent': self.w(0x30),
            'align_shift': self.w(0x32),
            'cres': self.w(0x34),
            'exetyp': self.d[self.ne + 0x36],
            'flagsother': self.d[self.ne + 0x37],
        }
        # NE-relative offsets -> absolute file offsets
        for k in ('enttab', 'rsrctab', 'restab', 'modtab', 'imptab', 'segtab'):
            h[k + '_file'] = self.ne + h[k]
        return h

    def segments(self):
        h = self.header()
        shift = 1 << h['align_shift']
        segs = []
        for i in range(h['cseg']):
            o, ln, fl, ma = struct.unpack_from('<HHHH', self.d, h['segtab_file'] + i * 8)
            segs.append({
                'n': i + 1,
                'sector': o,
                'file_off': o << h['align_shift'],
                'len': ln,
                'flags': fl,
                'minalloc': ma,
                'is_data': bool(fl & 0x0001),
                'is_nsiter': bool(fl & 0x0008),
                'is_movable': bool(fl & 0x0010),
                'is_preload': bool(fl & 0x0040),
                'is_discard': bool(fl & 0x1000),
                'ghidra_selector': 0x1000 + i * 8,
            })
        return segs, shift

    # ---------- simple tables ----------

    def resident_names(self):
        p = self.header()['restab_file']
        out = []
        while True:
            ln = self.d[p]
            if ln == 0:
                p += 1
                break
            name = self.d[p + 1:p + 1 + ln].decode('latin-1')
            ordi = struct.unpack_from('<H', self.d, p + 1 + ln)[0]
            out.append({'name': name, 'ordinal': ordi, 'at': hex(p)})
            p += 1 + ln + 2
        return out, p

    def module_refs(self):
        h = self.header()
        p = h['modtab_file']
        mods = []
        for i in range(h['cmod']):
            ln = self.d[p]
            if ln == 0:
                p += 1
                break
            name = self.d[p + 1:p + 1 + ln].decode('latin-1')
            mods.append({'idx': i + 1, 'name': name, 'at': hex(p)})
            p += 1 + ln
        # module ref table is usually terminated by a 0 byte
        if self.d[p] == 0:
            p += 1
        return mods, p

    def imported_names(self):
        h = self.header()
        p = h['imptab_file']
        end = h['nrestab_file']
        out = []
        while p < end:
            ln = self.d[p]
            if ln == 0:
                p += 1
                continue
            if ln & 0x80:
                ordi = struct.unpack_from('<H', self.d, p + 1)[0]
                out.append({'kind': 'ordinal', 'ordinal': ordi, 'at': hex(p)})
                p += 3
            else:
                name = self.d[p + 1:p + 1 + ln].decode('latin-1')
                out.append({'kind': 'name', 'name': name, 'at': hex(p)})
                p += 1 + ln
        return out, p

    def entry_table(self):
        h = self.header()
        p = h['enttab_file']
        end = p + h['cbenttab']
        out = []
        while p < end:
            cnt = self.d[p]
            if cnt == 0:
                p += 1
                continue
            seg = self.d[p + 1]
            p += 2
            for _ in range(cnt):
                fl = self.d[p]
                p += 1
                if fl & 0x02:          # has 2-byte offset
                    off = struct.unpack_from('<H', self.d, p)[0]
                    p += 2
                elif fl & 0x03:        # has 1-byte offset
                    off = self.d[p]
                    p += 1
                else:                  # exported, no offset
                    off = None
                out.append({'seg': seg, 'flags': fl, 'off': off})
        return out

    # ---------- relocations ----------

    # fixup target types (high nibble of the type word's byte 1)
    TGT = {0: 'internal_ref', 1: 'import_ordinal', 2: 'import_name',
           3: 'osfixup', 4: 'import_ordinal', 5: 'import_name'}
    LOCT = {0: 'LOBYTE', 2: 'SEGMENT', 3: 'FAR_PTR', 5: 'OFFSET',
            11: 'FAR_PTR_48', 13: 'OFFSET_32'}

    def relocations(self, verbose=False):
        segs, shift = self.segments()
        mods, _ = self.module_refs()
        h = self.header()
        all_fix = []
        report = []
        for i, s in enumerate(segs):
            p = s['file_off'] + s['len']
            # next segment's aligned start bounds the reloc area
            if i + 1 < len(segs):
                limit = segs[i + 1]['file_off']
            else:
                limit = h['nrestab_file']
            start = p
            recs = []
            note = ''
            if p + 2 <= len(self.d):
                count = struct.unpack_from('<H', self.d, p)[0]
                raw_count = count
                # high bit => a "segment relocation" descriptor word follows
                if count & 0x8000:
                    count &= 0x7fff
                    p += 2
                    note = 'seghdr'
                p += 2
                for _ in range(count):
                    if p + 8 > len(self.d):
                        note += '|TRUNC'
                        break
                    t = struct.unpack_from('<H', self.d, p)[0]
                    off = struct.unpack_from('<H', self.d, p + 2)[0]
                    tf = struct.unpack_from('<H', self.d, p + 4)[0]
                    tg = struct.unpack_from('<H', self.d, p + 6)[0]
                    p += 8
                    additive = None
                    if tf & 0x0004:
                        additive = struct.unpack_from('<H', self.d, p)[0]
                        p += 2
                    recs.append({
                        'type': t & 0x000f,
                        'type_name': self.LOCT.get(t & 0x000f, '?%d' % (t & 0x000f)),
                        'chained': bool(t & 0x0400) or bool(tf & 0x0100),
                        'modifier': (t >> 8) & 0xff,
                        'offset': off,
                        'target_flags': hex(tf),
                        'target_type': (tf & 0x0f00) >> 8 if (tf & 0x0f00) else (tf >> 8) & 0xff,
                        'target': tg,
                        'additive': additive,
                        'rec_at': hex(p - 8),
                    })
            report.append({
                'seg': s['n'], 'data_end': hex(start), 'limit': hex(limit),
                'reloc_bytes_avail': limit - start, 'count_raw': raw_count if p else None,
                'count': len(recs), 'bytes_used': p - start,
                'slack': limit - p, 'note': note,
            })
            for r in recs:
                r['seg'] = s['n']
                r['ghidra_sel'] = s['ghidra_selector']
                r['mod_name'] = (mods[r['target'] - 1]['name']
                                 if 1 <= r['target'] <= len(mods) else None)
                all_fix.append(r)
        return all_fix, report


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else \
        '/home/cabal/Git/repos/innerspace-rea/extracted/shipdisk/INSPACE.exe'
    out = sys.argv[2] if len(sys.argv) > 2 else None
    ne = NE(path)
    h = ne.header()
    mods, modend = ne.module_refs()
    rnames, rend = ne.resident_names()
    inames, iend = ne.imported_names()
    entries = ne.entry_table()
    fix, rep = ne.relocations()

    result = {
        'header': h,
        'modules': mods,
        'resident_names': rnames,
        'imported_names': inames,
        'entry_table': entries,
        'reloc_report': rep,
        'fixups': fix,
        'anchors': {
            'resident_end': hex(rend),
            'module_end': hex(modend),
            'imports_end': hex(iend),
            'nrestab_file': hex(h['nrestab_file']),
        },
    }
    if out:
        with open(out, 'w') as f:
            json.dump(result, f, indent=1)
        print('wrote', out)
    else:
        print(json.dumps(result, indent=1))


if __name__ == '__main__':
    main()
