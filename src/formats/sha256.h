// SHA-256 (FIPS 180-4), for the asset store's table of known files (ADR 0002). The table
// holds the lowercase hex digests that `sha256sum` writes to original.sha256.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace is::fmt {

using Sha256Digest = std::array<std::uint8_t, 32>;

class Sha256 {
public:
    Sha256() { reset(); }

    void reset();
    void update(const std::uint8_t* p, std::size_t n);
    // Pads, returns the digest and resets, so the object can hash the next message.
    Sha256Digest finish();

private:
    void block(const std::uint8_t* p);

    std::array<std::uint32_t, 8> h_{};
    std::array<std::uint8_t, 64> buf_{};
    std::size_t used_ = 0;      // bytes in buf_
    std::uint64_t length_ = 0;  // message bytes so far
};

Sha256Digest sha256(const std::uint8_t* p, std::size_t n);

// Lowercase hex, 64 characters.
std::string to_hex(const Sha256Digest& d);
std::string sha256_hex(const std::uint8_t* p, std::size_t n);

}  // namespace is::fmt
