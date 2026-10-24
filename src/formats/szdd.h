// SZDD ("Microsoft Compress", COMPRESS.EXE) expansion. The shipdisk stores 14 of its files
// this way (MASTER.BO1, ENG_LANG.dl1, ...). Header and LZSS scheme: re/isre/szdd.py, the
// reference this reader matches byte for byte.
//
//   0..7   magic 53 5A 44 44 88 F0 27 33 ("SZDD" + 4 bytes)
//   8      compression mode, 'A' (0x41) = LZSS
//   9      the name character COMPRESS replaced with '_' (0 if none)
//   10..13 expanded size, little-endian DWORD
//   14..   LZSS stream: 4096-byte window of spaces, write position 4096 - 16; each flag byte
//          covers 8 codes, low bit first; a set bit is a literal byte, a clear bit a match
//          lo, hi at window position lo | (hi & 0xF0) << 4, length (hi & 0x0F) + 3.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "formats/bytes.h"

namespace is::fmt {

constexpr std::size_t kSzddHeaderSize = 14;

struct SzddHeader {
    std::uint8_t mode = 0;   // 'A' (0x41), the only mode the reader expands
    char last_char = 0;      // the original name's character COMPRESS replaced, 0 if none
    std::uint32_t size = 0;  // expanded size
};

struct SzddFile {
    std::vector<std::uint8_t> data;  // the expanded bytes
    char last_char = 0;              // as in SzddHeader. Every shipdisk file stores 0: the
                                     // distribution renames by extension (.BO1 for .BOD)
};

// True when the data starts with the 8-byte SZDD magic.
bool is_szdd(const std::uint8_t* p, std::size_t n);

SzddHeader parse_szdd_header(const std::uint8_t* p, std::size_t n);

// Throws FormatError on a bad header, a mode other than 'A', or a stream that ends before
// the declared size. Input after the declared size is ignored.
SzddFile szdd_expand(const std::uint8_t* p, std::size_t n);

}  // namespace is::fmt
