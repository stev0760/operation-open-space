// Resource records that the original's code loads as data: ISPACE.DLL #5500 100, #5600 135
// and #5000 150, and IGRAPHIC.DLL #6600. The loaders and layouts are in
// docs/re/formats.md#resource-records-loaded-as-data, the raw bytes in
// docs/re/resources.md#other-data-types. Fields whose meaning the docs leave [OPEN] stay
// raw bytes or are named by their offset.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "formats/ne.h"

namespace is::fmt {

// ---- ISPACE.DLL #5500 100: 92 palette colours
// ISPACE.DLL seg5:0x254c copies the first 0x114 bytes (92 three-byte entries) of the
// 288-byte resource; the 12 bytes after them are zero and unused. seg5:0x2626 copies entry
// k into entry 11 + k of the game's LOGPALETTE, byte 0 to peRed, 1 to peGreen and 2 to
// peBlue. What it writes to peFlags is [OPEN] here.

struct Rgb8 {
    std::uint8_t r = 0, g = 0, b = 0;
};

struct Palette5500 {
    static constexpr std::uint16_t kType = 5500, kId = 100;
    static constexpr std::size_t kCount = 92;  // 0x114 bytes
    static constexpr int kFirstIndex = 11;     // LOGPALETTE entries 11-102
    std::array<Rgb8, kCount> colours{};
};

Palette5500 parse_palette5500(const std::uint8_t* p, std::size_t n);
Palette5500 load_palette5500(const NeModule& ispace);

// ---- ISPACE.DLL #5600 135: the initial 8 x 8 word table
// INSPACE.EXE seg10:0x38e0 copies the resource's 64 words row by row into the 8 x 8 word
// table at DGROUP:0x3824, which a saved game stores at header +0x36. What the rows and
// columns index is [OPEN]. The words are signed; all lie within +-250.

struct Table5600 {
    static constexpr std::uint16_t kType = 5600, kId = 135;
    std::array<std::array<std::int16_t, 8>, 8> rows{};  // rows[0] = words 0-7
};

Table5600 parse_table5600(const std::uint8_t* p, std::size_t n);
Table5600 load_table5600(const NeModule& ispace);

// ---- GlobalSize
// GlobalSize returns a block's allocation size rounded up: Wine's krnl386 rounds to 32
// bytes, the Windows 3.x granularity is [OPEN] (docs/re/formats.md#conventions). The port
// computes such sizes with this function, never from the requested size
// (docs/re/win16-translation.md). `granularity` is a power of two of at most 64 KiB, one
// Win16 segment; anything else throws FormatError.
constexpr std::size_t kWineGlobalGranularity = 32;
constexpr std::size_t kMaxGlobalGranularity = 0x10000;
std::size_t global_size(std::size_t n, std::size_t granularity = kWineGlobalGranularity);

// ---- IGRAPHIC.DLL #6600: a 0x6c-byte header and 0x26-byte records
// ISPACE.DLL seg8:0x5b76 copies the first 0x6c bytes into a 0x6c-byte global block (the
// header). It allocates a second block (the body) of SizeofResource - GlobalSize(header)
// bytes and copies the resource from offset GlobalSize(header) into it (seg8:0x5c2c-0x5c6f).
// Both blocks are GlobalAlloc(0x42, ...), moveable and zero-filled. ISPACE ordinal 32, the
// scene renderer, treats the body as 0x26-byte records, count = GlobalSize(body) / 0x26
// (seg8:0x72-0x78). Wine's 32-byte rounding puts the body at 0x80. The body block ends in
// zeros up to its GlobalSize, and a record that reaches into them counts: under 32-byte
// rounding the five shipped bodies are whole multiples of 32 bytes and gain nothing, while
// 64-byte rounding gives #6600 115 a 25th record. What the records describe is [OPEN].

struct Scene6600 {
    static constexpr std::uint16_t kType = 6600;
    static constexpr std::uint16_t kIds[] = {100, 105, 110, 115, 120};  // the ids INSPACE passes
    static constexpr std::size_t kHeaderSize = 0x6c;
    static constexpr std::size_t kRecordSize = 0x26;

    std::array<std::uint8_t, kHeaderSize> header{};  // raw, fields below decoded from it
    // Header fields ordinal 32 reads and writes. +0x50 and +0x51 each select one of three
    // modes (6, 7 or 8) for the dwords +0x5c and +0x60; ordinal 32 can overwrite both
    // dwords with sign-extended word arguments. The modes' meanings are [OPEN].
    std::uint8_t byte_50 = 0;
    std::uint8_t byte_51 = 0;
    std::int32_t dword_5c = 0;
    std::int32_t dword_60 = 0;

    std::size_t body_offset = 0;  // GlobalSize of the header block
    std::size_t body_length = 0;  // bytes copied into the body: n - body_offset
    // The body block, GlobalSize(body_length) bytes: the resource from body_offset to its
    // end, then zeros.
    std::vector<std::uint8_t> body;
    // body.size() / 0x26 records. Ordinal 32 adds an argument to body word +0x51 when the
    // body holds more than two records and body byte +0x6b is 0xec.
    std::vector<std::array<std::uint8_t, kRecordSize>> records;
};

// `n` is the resource length as the NE table gives it (what SizeofResource returns).
// `granularity` is GlobalSize's rounding unit (see global_size).
Scene6600 parse_scene6600(const std::uint8_t* p, std::size_t n,
                          std::size_t granularity = kWineGlobalGranularity);
Scene6600 load_scene6600(const NeModule& igraphic, std::uint16_t id,
                         std::size_t granularity = kWineGlobalGranularity);

// ---- ISPACE.DLL #5000 150: 88 distinct non-zero bytes, then zero padding
// No FindResource in INSPACE.EXE or ISPACE.DLL names it; its reader is [OPEN], so it stays raw.

struct Bytes5000 {
    static constexpr std::uint16_t kType = 5000, kId = 150;
    std::vector<std::uint8_t> bytes;  // the whole resource, padding included
    std::size_t length = 0;           // bytes before the trailing zeros
};

Bytes5000 parse_bytes5000(const std::uint8_t* p, std::size_t n);
Bytes5000 load_bytes5000(const NeModule& ispace);

}  // namespace is::fmt
