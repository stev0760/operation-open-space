#!/usr/bin/env python3
"""
Entry-redirect trick for single-entry 16-bit analyzers.

A headerless COM image gets exactly one entry point (file offset 0), so an
analyzer like Ghidra will only discover the one function reachable from it. To
analyze an arbitrary function in a segment, copy the segment and overwrite the
FIRST THREE BYTES with a near JMP to the target.

Why this is safe: every other byte keeps its original offset, so all segment-
relative absolute references (e.g. `les 0x1e30`, `lcall` offsets) stay valid. Only
the 3 overwritten bytes change, i.e. only the function that happened to live at
offset 0 - which should already be analyzed before you do this.

Usage: redirect_entry.py <segNN.bin> <target_offset_hex> [out.bin]
"""
import struct
import sys

src = sys.argv[1]
target = int(sys.argv[2], 16)
out = sys.argv[3] if len(sys.argv) > 3 else '/tmp/redirected.bin'

b = bytearray(open(src, 'rb').read())

if target + 3 > len(b):
    sys.exit('target 0x%x beyond segment length 0x%x' % (target, len(b)))

# E9 rel16 : rel = target - (0 + 3), the address after the jmp instruction
rel = (target - 3) & 0xFFFF
b[0] = 0xE9
struct.pack_into('<H', b, 1, rel)

assert len(b) == len(open(src, 'rb').read()), 'length must be preserved'
open(out, 'wb').write(bytes(b))

print('src            : %s (%d bytes)' % (src, len(b)))
print('entry redirect : 0x0000 -> E9 %04x = JMP 0x%04x' % (rel, target))
print('out            : %s' % out)
print('first 8 bytes  : %s' % b[:8].hex(' '))
print('bytes changed  : 3 (offsets 0..2 only; length unchanged)')
