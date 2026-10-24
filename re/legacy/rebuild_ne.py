#!/usr/bin/env python3
"""
Rebuild a clean (unpacked) NE executable from decompressed segments.

Takes the original packed INSPACE.exe and the unpacked segment images,
replaces each NSITER segment's data with the decompressed version,
clears the NSITER flag, and updates segment lengths.

The result is a standard NE that Ghidra/r2 can analyze without the OptLoader.

Usage: python3 rebuild_ne.py [packed.exe] [unpacked_dir] [output.exe]
"""
import struct
import os
import sys


def rebuild_ne(packed_path: str, unpacked_dir: str, output_path: str):
    d = bytearray(open(packed_path, 'rb').read())
    ne_off = struct.unpack_from('<H', d, 0x3c)[0]
    assert d[ne_off:ne_off + 2] == b'NE', 'Not NE'

    cseg = struct.unpack_from('<H', d, ne_off + 0x1c)[0]
    segtab = struct.unpack_from('<H', d, ne_off + 0x22)[0]
    align = struct.unpack_from('<H', d, ne_off + 0x32)[0]
    shift = 1 << align

    NSITER = 0x0008

    # Collect all segment info
    segs = []
    for i in range(cseg):
        off, ln, fl, ma = struct.unpack_from('<HHHH', d, ne_off + segtab + i * 8)
        segs.append({
            'n': i + 1,
            'sector': off,
            'file_off': off << align,
            'len': ln,
            'flags': fl,
            'minalloc': ma,
        })

    # The segments are stored contiguously in the file, each aligned to `shift` bytes.
    # We need to rebuild the file with new segment data.
    # Strategy: build a new file from scratch:
    #   1. Copy the header (MZ + NE header + tables) up to the first segment
    #   2. Write each segment's decompressed data at the proper aligned offset
    #   3. Update the segment table with new lengths and cleared NSITER flags
    #   4. Copy any non-segment data (resources, name tables, version info) from original

    first_seg_off = min(s['file_off'] for s in segs)
    header_end = first_seg_off  # Everything before first segment is header/tables

    # Find the end of the last segment in the original file
    last_seg_end = max(s['file_off'] + s['len'] for s in segs)
    # What comes after the last segment? (resources, version info, non-resident name table)
    tail_start = last_seg_end
    # Align tail_start up
    if tail_start % shift:
        tail_start = ((tail_start // shift) + 1) * shift
    # Actually, resources and other data might be BETWEEN segments or after.
    # Let's check: the NE header says where resources, restab, nrestab are.
    rsrctab_ne = struct.unpack_from('<H', d, ne_off + 0x24)[0]
    restab_ne = struct.unpack_from('<H', d, ne_off + 0x26)[0]
    nrestab_file = struct.unpack_from('<I', d, ne_off + 0x2c)[0]

    # For simplicity: copy everything from the original file up to first_seg_off,
    # then write new segments, then copy everything from the original tail.
    # But we need to handle the case where segments grow (decompressed > compressed).

    # New approach: build the output as:
    #   [header: 0 .. first_seg_off] (unchanged)
    #   [seg1 data][pad][seg2 data][pad]...[seg19 data][pad]
    #   [tail: resources, version info, etc. from original]

    # Load decompressed segments
    decomp = {}
    for i in range(1, cseg + 1):
        path = os.path.join(unpacked_dir, f'seg{i:02d}.bin')
        if os.path.exists(path):
            decomp[i] = open(path, 'rb').read()
        else:
            # Segment wasn't decompressed (seg1 = loader, not compressed)
            s = segs[i - 1]
            decomp[i] = bytes(d[s['file_off']:s['file_off'] + s['len']])

    # For NSITER segments, use decompressed data (truncated/padded to len or minalloc)
    # For non-NSITER segments, keep original data
    new_segs = []
    for s in segs:
        n = s['n']
        data = decomp.get(n, b'')
        if s['flags'] & NSITER:
            # Clear NSITER flag
            new_flags = s['flags'] & ~NSITER
            # New length = len of decompressed data (but not more than minalloc)
            # Actually: the segment file size should be the code size (not minalloc/BSS)
            # The decompressed data includes code + initialized data. BSS is zero-filled at load.
            # For a clean NE: len = min(len(decomp), some reasonable size)
            # Let's use the full decompressed size (capped at 64K for NE segments)
            new_len = min(len(data), 0xFFFF)
            if new_len == 0:
                new_len = s['len']  # fallback
            new_segs.append({
                'n': n,
                'data': data[:new_len],
                'len': new_len,
                'flags': new_flags,
                'minalloc': s['minalloc'],
            })
        else:
            new_segs.append({
                'n': n,
                'data': data,
                'len': s['len'],
                'flags': s['flags'],
                'minalloc': s['minalloc'],
            })

    # Compute new file layout
    # Each segment starts at an aligned offset
    out = bytearray()
    # Copy header
    out.extend(d[:first_seg_off])

    new_seg_entries = []
    current_off = first_seg_off
    for ns in new_segs:
        # Align current offset
        if current_off % shift:
            pad = shift - (current_off % shift)
            out.extend(b'\x00' * pad)
            current_off += pad

        new_sector = current_off >> align
        new_seg_entries.append({
            'sector': new_sector,
            'len': ns['len'],
            'flags': ns['flags'],
            'minalloc': ns['minalloc'],
        })

        out.extend(ns['data'])
        current_off += len(ns['data'])

    # Pad to alignment
    if current_off % shift:
        pad = shift - (current_off % shift)
        out.extend(b'\x00' * pad)
        current_off += pad

    new_tail_start = current_off

    # Copy tail from original (everything after last segment, aligned)
    # The tail includes: resources, version info, non-resident name table, etc.
    # In the original, tail starts at the aligned offset after the last segment.
    orig_tail_start = last_seg_end
    if orig_tail_start % shift:
        orig_tail_start = ((orig_tail_start // shift) + 1) * shift

    tail = bytes(d[orig_tail_start:])
    out.extend(tail)

    # Update segment table in the output
    for i, nse in enumerate(new_seg_entries):
        entry_off = ne_off + segtab + i * 8
        struct.pack_into('<HHHH', out, entry_off,
                         nse['sector'], nse['len'], nse['flags'], nse['minalloc'])

    # Update nrestab (non-resident name table file offset) — shift by (new_tail_start - orig_tail_start)
    tail_delta = new_tail_start - orig_tail_start
    if tail_delta != 0:
        struct.pack_into('<I', out, ne_off + 0x2c, nrestab_file + tail_delta)
        print(f'  nrestab: {nrestab_file:#x} -> {nrestab_file + tail_delta:#x} (delta={tail_delta:+d})')

    # Update NE flags: clear any "app loader" flag if present
    # NEAPPLOADER = 0x0800 — the NE flags field at +0x0c
    ne_flags = struct.unpack_from('<H', out, ne_off + 0x0c)[0]
    if ne_flags & 0x0800:
        print(f'  Clearing NEAPPLOADER flag (was {ne_flags:#06x})')
        ne_flags &= ~0x0800
        struct.pack_into('<H', out, ne_off + 0x0c, ne_flags)

    # The entry point: in the original, entry CS:IP points to seg5 (the OptLoader stub area
    # within the packed seg5). In the unpacked binary, seg5's decompressed data has the
    # REAL entry code. But the entry CS:IP in the NE header still points to the old location.
    # 
    # Actually: the OptLoader entry IS the NE entry. It loads and decompresses everything,
    # then jumps to the real app entry. In the unpacked binary, we want the NE entry to
    # point directly to the real app entry.
    #
    # The real app entry is determined by the OptLoader's BOOTAPP, which eventually calls
    # the app's WinMain/init. This is complex. For now, keep the original entry point
    # (seg1 = the loader) since seg1 is NOT compressed and still contains the loader stub.
    # 
    # Actually wait: seg1 is the OptLoader stub. In the unpacked binary, seg1 is unchanged
    # (not compressed). But segments 2-18 are now uncompressed. The OptLoader in seg1 will
    # try to "decompress" them again (since NSITER is now cleared, it won't). Good.
    # But the OptLoader also does relocation processing and memory allocation.
    # 
    # Hmm, this is getting complex. The OptLoader is a REPLACEMENT for the Windows NE loader.
    # It doesn't use the standard Windows loading mechanism. So even with NSITER cleared,
    # the standard Windows loader won't know how to load this binary because the OptLoader
    # intercepts the entry point.
    #
    # For Ghidra analysis, we don't need a runnable binary. We just need the code to be
    # in the right place with the right segment mapping. So let's just update the segment
    # table and keep everything else.

    # Zero the CRC
    struct.pack_into('<I', out, ne_off + 8, 0)

    with open(output_path, 'wb') as f:
        f.write(out)

    print(f'\nRebuilt NE: {output_path}')
    print(f'  Original size: {len(d)} bytes')
    print(f'  New size: {len(out)} bytes')
    print(f'  Tail delta: {tail_delta:+d} bytes')
    print(f'  Segments: {len(new_seg_entries)}')
    for i, nse in enumerate(new_seg_entries):
        s = segs[i]
        changed = ' (was NSITER)' if (s['flags'] & NSITER) else ''
        print(f'    seg{i+1:2d}: file_off={nse["sector"] << align:#08x} len={nse["len"]:6d} (was {s["len"]:6d}) flags={nse["flags"]:#06x}{changed}')

    return bytes(out)


if __name__ == '__main__':
    packed = sys.argv[1] if len(sys.argv) > 1 else \
        '/home/cabal/Git/repos/innerspace-rea/extracted/shipdisk/INSPACE.exe'
    unpacked_dir = sys.argv[2] if len(sys.argv) > 2 else \
        '/home/cabal/Git/repos/innerspace-rea/unpacked'
    output = sys.argv[3] if len(sys.argv) > 3 else \
        '/home/cabal/Git/repos/innerspace-rea/INSPACE-unpacked.exe'

    rebuild_ne(packed, unpacked_dir, output)
