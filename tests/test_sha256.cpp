// SHA-256: the FIPS 180-4 example vectors, the padding boundaries and incremental updates.
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "formats/sha256.h"

using namespace is::fmt;

namespace {

std::string hex_of(const std::string& s) {
    return sha256_hex(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

// Bytes (7i + 3) mod 256: no repeats within a block, so a misplaced byte shows.
std::vector<std::uint8_t> pattern(std::size_t n) {
    std::vector<std::uint8_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::uint8_t>(i * 7 + 3);
    return v;
}

}  // namespace

TEST_CASE("SHA-256: the FIPS 180-4 example messages") {
    CHECK(hex_of("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(hex_of("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(hex_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(hex_of("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
                 "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu") ==
          "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    CHECK(hex_of(std::string(1000000, 'a')) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("SHA-256: lengths around the padding boundaries") {
    // 55 bytes leave room for the 0x80 and the length in one block, 56 need a second
    const std::pair<std::size_t, const char*> want[] = {
        {55, "e7313d333c272e639f790978283f9eb392e843d0f29b7016828bb1daa4aac70b"},
        {56, "4324d65f3c103567f5589c710bc08f8523f929a9272e3af36fc968e52abc6c27"},
        {63, "81c80242132f230c3bd41b3e63bbcff16107339549214a99614ff26664625055"},
        {64, "39e3d7b6b5d075d37d053ad89b24b41bef4f3c29760c84447cab3f3be1882241"},
        {65, "aacca6ff74fdbb296d165a45cecfa04e5127bc008770fbbdd48006f2d2fae95e"},
        {119, "9ce7368e4daf32341631b492e80359dc9f594b48453cd0dd5bf0b19279cc177e"},
        {120, "7836b787757e95e58b3ca5aec90b1b004e8deba1e50e9675af9cabf1a13a04b5"},
    };
    for (const auto& [n, hex] : want) {
        const auto m = pattern(n);
        CHECK_MESSAGE(sha256_hex(m.data(), m.size()) == hex, n << " bytes");
    }
}

TEST_CASE("SHA-256: updates split anywhere give the one-shot digest, and finish resets") {
    const auto m = pattern(200);
    const Sha256Digest one = sha256(m.data(), m.size());
    for (std::size_t a = 0; a <= m.size(); a += 7)
        for (std::size_t b = a; b <= m.size(); b += 13) {
            Sha256 s;
            s.update(m.data(), a);
            s.update(m.data() + a, b - a);
            s.update(m.data() + b, m.size() - b);
            CHECK_MESSAGE(s.finish() == one, a << ", " << b);
        }
    Sha256 s;
    for (std::uint8_t c : m) s.update(&c, 1);
    CHECK(s.finish() == one);
    // finish leaves the object ready for the next message
    s.update(reinterpret_cast<const std::uint8_t*>("abc"), 3);
    CHECK(to_hex(s.finish()) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(to_hex(s.finish()) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}
