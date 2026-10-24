#!/usr/bin/env python3
"""
Decide the two open questions in the DPMI_UNQUIK port, empirically, by
reproducing the known-good segment images in unpacked/segNN.bin.

Q1: start offset of the compressed bitstream  (fo+2 vs fo+4)
Q2: bit polarity (1=literal per nappload.asm vs 0=literal per slr_unpack.py)

Reference images were produced by slr_decompress_v3.py, which is the version
BREAKTHROUGH.md records as VERIFIED and whose output matched the relocation
item counts exactly -- so reproducing them is the correctness test.
"""
import struct
import sys
import os

PROJ = '/home/cabal/Git/repos/innerspace-rea'
EXE = os.path.join(PROJ, 'extracted/shipdisk/INSPACE.exe')


def unquik(src, max_out, polarity_one_is_literal):
    """Faithful port of DPMI_UNQUIK from nappload.asm, with a polarity switch."""
    out = bytearray()
    si = 0
    bp = 0
    dx = 0
    cx = 0

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

    def read_byte():
        nonlocal si
        b = src[si] if si < len(src) else 0
        si += 1
        return b

    def emit_match(bh, bl, n):
        bx = ((bh << 8) | bl) & 0xFFFF
        p = (((~bx) & 0xFFFF) + len(out)) & 0xFFFF
        for _ in range(n):
            out.append(out[p] if 0 <= p < len(out) else 0)
            p = (p + 1) & 0xFFFF

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
        c = 0
        for _ in range(bl):
            c = ((c << 1) | shift_bit()) & 0xFFFF
        return (c & 0xFF) + bh

    load_word()
    cx = 0
    state = '1'
    bh = 0
    saved = 0

    while len(out) < max_out:
        if state == '1':
            bit = shift_bit()
            is_lit = (bit == 1) if polarity_one_is_literal else (bit == 0)
            if is_lit:
                out.append(read_byte())
                state = '1'
            else:
                state = '2'
        elif state == '2':
            bit = shift_bit()
            goes3 = (bit == 1) if polarity_one_is_literal else (bit == 0)
            if goes3:
                state = '3'
            else:
                bh = 0
                cx = 1
                state = '261_2'
        elif state == '261_2':
            cx = (cx + cx + shift_bit()) & 0xFFFF
            if cx == 2:
                emit_match(bh, read_byte(), cx)
                state = '1'
            else:
                state = '5'
        elif state == '3':
            cx = 1
            bit = shift_bit()
            if bit == 0:
                bh = 0
                cx = 2
                state = '261_3'
            else:
                cx = 2
                bit = shift_bit()
                if bit == 0:
                    bh = 0
                    cx = 3
                    state = '261_3'
                else:
                    state = '111'
        elif state == '261_3':
            cx = (cx + cx + shift_bit()) & 0xFFFF
            state = '5'
        elif state == '5':
            bh = 0
            saved = cx
            if shift_bit() == 1:
                state = '6'
            else:
                if shift_bit() == 1:
                    state = '55'
                else:
                    emit_match(0, read_byte(), saved)
                    state = '1'
        elif state == '55':
            bh = 1
            if shift_bit() == 0:
                emit_match(1, read_byte(), saved)
                state = '1'
            else:
                bh = get_range_cx(0x0201)
                emit_match(bh, read_byte(), saved)
                state = '1'
        elif state == '6':
            if shift_bit() == 1:
                state = '7'
            else:
                bh = get_range_cx(0x0402) if shift_bit() == 0 else get_range_cx(0x0803)
                emit_match(bh, read_byte(), saved)
                state = '1'
        elif state == '7':
            if shift_bit() == 0:
                bh = get_range_cx(0x1004)
            elif shift_bit() == 0:
                bh = get_range_cx(0x2004)
            elif shift_bit() == 0:
                bh = get_range_cx(0x3004)
            else:
                bh = get_range_cx(0x4006)
            emit_match(bh, read_byte(), saved)
            state = '1'
        elif state == '111':
            if shift_bit() == 0:
                cx = get_bx(0x0802)
                state = '5'
            else:
                state = '1111'
        elif state == '1111':
            if shift_bit() == 0:
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
                break
        else:
            raise ValueError(state)
    return bytes(out), si


def main():
    d = open(EXE, 'rb').read()
    ne = struct.unpack_from('<H', d, 0x3c)[0]
    segtab = struct.unpack_from('<H', d, ne + 0x22)[0]
    align = struct.unpack_from('<H', d, ne + 0x32)[0]

    print('%-6s %8s %8s  %-34s %-34s' %
          ('seg', 'fo', 'minalloc', 'v3-offset4 (1=literal)', 'offset2 (1=literal)'))
    for n in (2, 3, 4, 5, 6, 9, 15, 19):
        off, ln, fl, ma = struct.unpack_from('<HHHH', d, ne + segtab + (n - 1) * 8)
        fo = off << align
        ref_path = os.path.join(PROJ, 'unpacked', 'seg%02d.bin' % n)
        ref = open(ref_path, 'rb').read()
        w0 = struct.unpack_from('<H', d, fo)[0]
        row = []
        for start in (4, 2):
            src = d[fo + start:fo + ln]
            try:
                got, si = unquik(src, ma + 512, True)
            except Exception as e:
                row.append('EXC %s' % e)
                continue
            # compare against reference (reference is zero-padded to minalloc)
            k = 0
            while k < min(len(got), len(ref)) and got[k] == ref[k]:
                k += 1
            verdict = ('MATCH len=%d' % len(got)) if (len(got) == len(ref) and k == len(ref)) \
                else ('len=%d/%d sameprefix=%d' % (len(got), len(ref), k))
            row.append(verdict)
        print('%-6d %8s %8d  w0=%-4d %-26s %-34s' % (n, hex(fo), ma, w0, row[0], row[1]))


if __name__ == '__main__':
    main()
