#!/usr/bin/env python3
"""SZDD (Microsoft Compress, LZSS 'A' variant) decompressor — corrected.

Layout (verified against shipped files):
  0..7   magic SZDD\x88\xF0\x27\x33
  8      fmt 'A'
  9      0x00
  10..13 original size LE32
  14..   LZSS stream: flag byte, then 8 codes LSB-first (bit set = literal)
LZSS: 4096-byte window init 0x20, start pos 4096-16; match: lo,hi ->
offset = lo | ((hi & 0xF0) << 4), length = (hi & 0x0F) + 3 (3..18).
"""
import sys

def expand_szdd(data: bytes) -> bytes:
    if data[:8] != b"SZDD\x88\xF0\x27\x33":
        raise ValueError("not SZDD")
    if data[8] != 0x41:
        raise ValueError(f"unsupported fmt {data[8]:#x}")
    orig_size = int.from_bytes(data[10:14], "little")
    src = data[14:]
    out = bytearray()
    win = bytearray(b"\x20" * 4096)
    win_pos = 4096 - 16
    i = 0
    while i < len(src) and len(out) < orig_size:
        flags = src[i]; i += 1
        for bit in range(8):
            if len(out) >= orig_size or i >= len(src):
                break
            if flags & (1 << bit):
                out.append(src[i])
                win[win_pos] = src[i]
                win_pos = (win_pos + 1) & 0xFFF
                i += 1
            else:
                if i + 1 >= len(src):
                    break
                lo = src[i]; hi = src[i + 1]; i += 2
                off = lo | ((hi & 0xF0) << 4)
                length = (hi & 0x0F) + 3
                for _ in range(length):
                    if len(out) >= orig_size:
                        break
                    c = win[off & 0xFFF]
                    out.append(c)
                    win[win_pos] = c
                    win_pos = (win_pos + 1) & 0xFFF
                    off += 1
    return bytes(out)

if __name__ == "__main__":
    inp, outp = sys.argv[1], sys.argv[2]
    out = expand_szdd(open(inp, "rb").read())
    open(outp, "wb").write(out)
    print(f"{inp} -> {outp}: {len(out)} bytes")