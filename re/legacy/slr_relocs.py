#!/usr/bin/env python3
"""
Extract and decode SLR OptLoader relocation tables from an OptLoader-packed NE.

Derived from DigitalMars/optlink appload/nappload.asm (the loader's own source):

  DECOMPRESS:
      LODSW                 ; word0 = relocation ITEM COUNT  -> returned in DX
      CALL DPMI_UNQUIK      ; decompress the segment image
      BX = SI               ; SI now points at the relocation table
                            ; (relocs live in the on-disk segment data,
                            ;  immediately after the compressed bitstream)

  PROCESS_RELOCS:  a sequence of packets.
      w = LODSW ; count = w >> 8 ; al = w & 0xFF
      if al == 0xF0:                        ; "special base type"
          count x { byte selector_idx, word offset }
      else:
          target_type = al & 0x07           ; TARG_TYPE_TBL
          source_type = (al >> 3) & 0x03    ; SOURCE_TYPE_TBL
          INTERNAL(0): LODSB al2
                       al2 != 0xFF -> fixed segment ref, then count x {word off, word val}
                       al2 == 0xFF -> movable (own selector), count x {word off, word val}
          ORDINAL (1): LODSW module_idx,      then count x {word off, word ORDINAL}
          NAME    (2): LODSW module_idx,      then count x {word off, word imptab_off}
          OSFLOAT (3): LODSW n,               then count x {word off}

Target types (what the loader writes at the fixup site):
  0 BYTE  1 SEG16  2 PTR16  3 OFF16  4 BYTE_A  5 SEG16_A  6 PTR16_A  7 OFF16_A

Usage: python3 slr_relocs.py [in.exe] [out.json]
"""
import struct
import os
import sys
import json
from collections import Counter

PROJ = '/home/cabal/Git/repos/innerspace-rea'

TARG_NAMES = ['BYTE', 'SEG16', 'PTR16', 'OFF16',
              'BYTE_A', 'SEG16_A', 'PTR16_A', 'OFF16_A']
SRC_NAMES = ['INTERNAL', 'ORDINAL', 'NAME', 'OSFLOAT']


# ───────────────────────── DPMI_UNQUIK (traced) ─────────────────────────

def dpmi_unquik_trace(src, max_out=0x10000):
    """Port of DPMI_UNQUIK. Returns (output_bytes, si_after_stream).

    si_after_stream is the index into `src` immediately past the end-of-stream
    marker byte, i.e. the start of the relocation table (relative to src[0],
    which corresponds to (segment_file_off + 2)).
    """
    out = bytearray()
    si = 0
    bp = 0
    dx = 0

    def load_word():
        nonlocal si, bp, dx
        if si + 1 < len(src):
            bp = src[si] | (src[si + 1] << 8)
        elif si < len(src):
            bp = src[si]
        else:
            bp = 0
        si += 2
        dx = 16

    def shift_bit():
        nonlocal bp, dx
        carry = (bp >> 15) & 1
        bp = (bp << 1) & 0xFFFF
        dx -= 1
        if dx == 0:
            load_word()
        return carry

    def get_bit():
        return shift_bit()

    def read_byte():
        nonlocal si
        b = src[si] if si < len(src) else 0
        si += 1
        return b

    def do_27(bh, bl, match_len):
        bx = ((bh << 8) | bl) & 0xFFFF
        src_pos = ((~bx) & 0xFFFF) + len(out)
        src_pos &= 0xFFFF
        for _ in range(match_len):
            out.append(out[src_pos] if 0 <= src_pos < len(out) else 0)
            src_pos = (src_pos + 1) & 0xFFFF

    def get_range_cx(cx_init):
        cl = cx_init & 0xFF
        ch = (cx_init >> 8) & 0xFF
        bh = 0
        for _ in range(cl):
            bh = ((bh << 1) | shift_bit()) & 0xFF
        return (bh + ch) & 0xFF

    def get_bx(bx_init):
        bl = bx_init & 0xFF
        bh = (bx_init >> 8) & 0xFF
        cx = 0
        for _ in range(bl):
            cx = ((cx << 1) | shift_bit()) & 0xFFFF
        return (cx & 0xFF) + bh

    load_word()
    cx = 0
    state = '1'
    bh = 0
    saved_cx = 0

    while len(out) < max_out:
        if state == '1':
            # 1$: ADD BP,BP; DEC DX; JZ 15$; JC 0$  -- carry set => literal byte
            if shift_bit() == 1:
                out.append(read_byte())
            else:
                state = '2'
        elif state == '2':
            # 2$: ADD BP,BP; DEC DX; JZ 25$; JC 3$
            if shift_bit() == 1:
                state = '3'
            else:
                bh = 0
                cx = 1
                state = '261_from_2'
        elif state == '261_from_2':
            cx = (cx + cx + shift_bit()) & 0xFFFF
            if cx == 2:
                do_27(bh, read_byte(), cx)
                state = '1'
            else:
                state = '5'
        elif state == '3':
            cx = 1
            if shift_bit() == 0:
                bh = 0
                cx = 2
                state = '261_from_3'
            else:
                cx = 2
                if shift_bit() == 0:
                    bh = 0
                    cx = 3
                    state = '261_from_3'
                else:
                    state = '111'
        elif state == '261_from_3':
            cx = (cx + cx + shift_bit()) & 0xFFFF
            state = '5'
        elif state == '5':
            bh = 0
            saved_cx = cx
            if shift_bit() == 1:
                state = '6'
            else:
                if shift_bit() == 1:
                    state = '55'
                else:
                    bl = read_byte()
                    do_27(bh, bl, saved_cx)
                    state = '1'
        elif state == '55':
            bh = 1
            if shift_bit() == 0:
                do_27(bh, read_byte(), saved_cx)
                state = '1'
            else:
                bh = get_range_cx(0x0201)
                do_27(bh, read_byte(), saved_cx)
                state = '1'
        elif state == '6':
            if get_bit() == 1:
                state = '7'
            else:
                bh = get_range_cx(0x0402) if get_bit() == 0 else get_range_cx(0x0803)
                do_27(bh, read_byte(), saved_cx)
                state = '1'
        elif state == '7':
            if get_bit() == 0:
                bh = get_range_cx(0x1004)
            else:
                if get_bit() == 0:
                    bh = get_range_cx(0x2004)
                else:
                    bh = get_range_cx(0x3004) if get_bit() == 0 else get_range_cx(0x4006)
            do_27(bh, read_byte(), saved_cx)
            state = '1'
        elif state == '111':
            if get_bit() == 0:
                cx = get_bx(0x0802)
                state = '5'
            else:
                state = '1111'
        elif state == '1111':
            if get_bit() == 0:
                cx = get_bx(0x0C03)
                state = '5'
            else:
                state = '11111'
        elif state == '11111':
            cl = read_byte()
            if cl < 0x81:
                cx = cl
                state = '5'
            elif cl == 0x81:
                cx = 0
                state = '1'
            else:
                break          # end of stream: SI is just past this byte
    return bytes(out), si


# ───────────────────────── NE parsing ─────────────────────────

def parse_ne(d):
    ne = struct.unpack_from('<H', d, 0x3c)[0]
    assert d[ne:ne + 2] == b'NE'
    g = lambda o: struct.unpack_from('<H', d, ne + o)[0]
    segs = []
    for i in range(g(0x1c)):
        off, ln, fl, ma = struct.unpack_from('<HHHH', d, ne + g(0x22) + i * 8)
        segs.append({'n': i + 1, 'file_off': off << g(0x32), 'len': ln,
                     'flags': fl, 'minalloc': ma, 'nsiter': bool(fl & 0x0008)})
    return {'ne': ne, 'cseg': g(0x1c), 'cmod': g(0x1e), 'align': g(0x32),
            'modtab': g(0x28), 'imptab': g(0x2a), 'segtab': g(0x22), 'segments': segs}


# ───────────────────────── reloc packet decoder ─────────────────────────

def decode_relocs(buf, start, total_count, seg_n, verbose=False):
    """buf = on-disk segment bytes; start = offset of reloc table.
    Returns (records, consumed_bytes, errors)."""
    p = start
    remaining = total_count
    recs = []
    errs = []
    guard = 0
    while remaining > 0:
        guard += 1
        if guard > 100000:
            errs.append('packet guard tripped')
            break
        if p + 2 > len(buf):
            errs.append('packet header past end of buffer')
            break
        w = struct.unpack_from('<H', buf, p)[0]
        p += 2
        count = (w >> 8) & 0xFF
        al = w & 0xFF
        if count == 0:
            errs.append('zero-count packet at 0x%x' % (p - 2))
            break
        remaining -= count

        if al == 0xF0:
            for _ in range(count):
                if p + 3 > len(buf):
                    errs.append('short special-base item')
                    break
                sel = buf[p]
                off = struct.unpack_from('<H', buf, p + 1)[0]
                p += 3
                recs.append({'seg': seg_n, 'kind': 'special_base',
                             'target_type': 'SEG16_from_idx', 'offset': off,
                             'selector_idx': sel})
            continue

        ttype = al & 0x07
        stype = (al >> 3) & 0x03

        if stype == 0:                      # INTERNAL
            al2 = buf[p]
            p += 1
            ref = al2
            for _ in range(count):
                off = struct.unpack_from('<H', buf, p)[0]
                val = struct.unpack_from('<H', buf, p + 2)[0]
                p += 4
                recs.append({'seg': seg_n, 'kind': 'internal',
                             'target_type': TARG_NAMES[ttype], 'offset': off,
                             'ref_seg_raw': ref, 'value': val})
        elif stype == 1:                    # ORDINAL (the import case)
            mod = struct.unpack_from('<H', buf, p)[0]
            p += 2
            for _ in range(count):
                off = struct.unpack_from('<H', buf, p)[0]
                ordi = struct.unpack_from('<H', buf, p + 2)[0]
                p += 4
                recs.append({'seg': seg_n, 'kind': 'import_ordinal',
                             'target_type': TARG_NAMES[ttype], 'offset': off,
                             'module_idx': mod, 'ordinal': ordi})
        elif stype == 2:                    # NAME
            mod = struct.unpack_from('<H', buf, p)[0]
            p += 2
            for _ in range(count):
                off = struct.unpack_from('<H', buf, p)[0]
                nm = struct.unpack_from('<H', buf, p + 2)[0]
                p += 4
                recs.append({'seg': seg_n, 'kind': 'import_name',
                             'target_type': TARG_NAMES[ttype], 'offset': off,
                             'module_idx': mod, 'imptab_off': nm})
        else:                               # OSFLOAT
            n = struct.unpack_from('<H', buf, p)[0]
            p += 2
            for _ in range(count):
                off = struct.unpack_from('<H', buf, p)[0]
                p += 2
                recs.append({'seg': seg_n, 'kind': 'osfloat',
                             'target_type': TARG_NAMES[ttype], 'offset': off,
                             'n': n})
    return recs, p - start, errs


# ───────────────────────── main ─────────────────────────

def main():
    inp = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(PROJ, 'extracted/shipdisk/INSPACE.exe')
    outp = sys.argv[2] if len(sys.argv) > 2 else os.path.join(PROJ, 'relocs.json')

    d = open(inp, 'rb').read()
    ne = parse_ne(d)
    print('NE @ %#x  cseg=%d cmod=%d align=%d' %
          (ne['ne'], ne['cseg'], ne['cmod'], ne['align']))
    print()

    all_recs = []
    report = []
    for s in ne['segments']:
        fo, ln, n = s['file_off'], s['len'], s['n']
        if not s['nsiter']:
            report.append({'seg': n, 'nsiter': False, 'items': 0})
            continue
        w0 = struct.unpack_from('<H', d, fo)[0]
        segdata = d[fo:fo + ln]
        out, si = dpmi_unquik_trace(segdata[2:], max_out=s['minalloc'] + 4096)
        reloc_start = 2 + si
        avail = ln - reloc_start
        if w0 == 0:
            report.append({'seg': n, 'nsiter': True, 'items': 0,
                           'decomp': len(out), 'minalloc': s['minalloc'],
                           'reloc_start': reloc_start, 'avail': avail})
            continue
        recs, used, errs = decode_relocs(segdata, reloc_start, w0, n)
        all_recs.extend(recs)
        report.append({
            'seg': n, 'nsiter': True, 'items': w0, 'decoded': len(recs),
            'decomp': len(out), 'minalloc': s['minalloc'],
            'reloc_start': reloc_start, 'reloc_bytes_avail': avail,
            'reloc_bytes_used': used, 'errs': errs,
            'kinds': dict(Counter(r['kind'] for r in recs)),
        })

    print('%-5s %6s %8s %8s %9s %9s %9s  %s' %
          ('seg', 'items', 'decoded', 'decomp', 'minalloc', 'reloc@', 'bytes',
           'kinds'))
    for r in report:
        if not r.get('nsiter'):
            print('%-5d (plain segment, not packed)' % r['seg'])
            continue
        print('%-5d %6d %8s %8d %9d %9d %9d  %s' %
              (r['seg'], r['items'], r.get('decoded', '-'), r['decomp'],
               r['minalloc'], r.get('reloc_start', 0),
               r.get('reloc_bytes_avail', r.get('avail', 0)),
               r.get('kinds', {})))

    print('\nTOTAL reloc records decoded: %d' % len(all_recs))
    print('by kind:', dict(Counter(r['kind'] for r in all_recs)))
    print('by target type:', dict(Counter(r['target_type'] for r in all_recs)))

    mods = {}
    # module names: module ref table = words, offsets into imported names table
    imptab_file = ne['ne'] + ne['imptab']
    p = ne['ne'] + ne['modtab']
    for i in range(ne['cmod']):
        o = struct.unpack_from('<H', d, p + i * 2)[0]
        q = imptab_file + o
        l = d[q]
        mods[i + 1] = d[q + 1:q + 1 + l].decode('latin-1')
        print('  module[%d] = %s' % (i + 1, mods[i + 1]))

    for r in all_recs:
        if 'module_idx' in r:
            r['module'] = mods.get(r['module_idx'], '?%d' % r['module_idx'])

    json.dump({'modules': mods, 'report': report, 'relocs': all_recs},
              open(outp, 'w'), indent=1)
    print('\nwrote', outp)


if __name__ == '__main__':
    main()
