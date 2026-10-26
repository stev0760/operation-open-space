// A minimal PNG writer for the exported manual's pictures (ADR 0005 decision 2). Deflate
// "stored" blocks only: no compression, so no zlib, and the output depends on nothing but
// the pixels [DESIGN]. Portable.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "formats/dib.h"

namespace is::help {

// CRC-32 (ISO 3309, as PNG chunks use it) and Adler-32 (zlib).
std::uint32_t crc32(const std::uint8_t* p, std::size_t n, std::uint32_t crc = 0);
std::uint32_t adler32(const std::uint8_t* p, std::size_t n);

// A PNG of the DIB: 8-bit indexed with the DIB's palette when bpp <= 8, else 8-bit RGB.
// Rows top-down.
std::vector<std::uint8_t> png_from_dib(const fmt::Dib& dib);

}  // namespace is::help
