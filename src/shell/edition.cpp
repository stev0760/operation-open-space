#include "shell/edition.h"

#include <algorithm>

namespace is::shell {

namespace {

std::int16_t w16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

std::uint16_t rd16(const std::vector<std::uint8_t>& b, std::size_t at) {
    const auto byte = [&](std::size_t i) -> unsigned { return i < b.size() ? b[i] : 0; };
    return static_cast<std::uint16_t>(byte(at) | (byte(at + 1) << 8));
}

constexpr std::uint8_t kNameKey[3] = {0x2A, 0x32, 0x2D};  // ISPACE.DLL seg10:0x4dc9
constexpr std::size_t kNameBytes = 50;                    // 0x32 [BYTES ISPACE.DLL seg10:0x4d45]

}  // namespace

int edition_day_value(int tm_year, int tm_yday) {
    // local_152 = tm_year - 0x5d; local_154 = local_152 * 0x16d + tm_yday [BYTES seg10:0x886d].
    return w16((tm_year - 93) * 365 + tm_yday);
}

// ISPACE.DLL seg10:0x4f9c(b, 0) [BYTES ISPACE.DLL seg10:0x4f9c seg10:0x5107 seg10:0x5118
// seg10:0x512f seg10:0x513c seg10:0x5157 seg10:0x5168 seg10:0x517e seg10:0x518b].
int record_check(const std::vector<std::uint8_t>& rec, int vidsp, int pdev) {
    const std::int16_t v = w16(vidsp), p = w16(pdev);
    if (w16(p - v) < 0) return 2;
    const std::uint16_t d_lo = rd16(rec, 0), d_hi = rd16(rec, 2);
    if (d_lo == 0 && d_hi == 0) return 3;
    // The serial seg10:0x4440 is 0 on the NT family [BYTES ISPACE.DLL seg10:0x4452]; the port
    // takes that branch on every Windows (ADR 0008 decision 2) [DESIGN].
    if (p == v) return 1;
    // seg10:0x51b6 reads both values again and compares the words [BYTES ISPACE.DLL
    // seg10:0x5243 seg10:0x5252].
    const std::uint16_t want = static_cast<std::uint16_t>(d_lo % 653 + 654);
    if (static_cast<std::uint16_t>(p - v) == want) return 0;
    return 1000;
}

std::vector<std::uint8_t> descramble_name(std::vector<std::uint8_t> b) {
    for (std::size_t i = 0; i < b.size(); ++i)  // [BYTES ISPACE.DLL seg10:0x4ddb seg10:0x4e13]
        b[i] = b[i] >= 0x80 ? 0 : static_cast<std::uint8_t>(b[i] ^ kNameKey[i % 3]);
    return b;
}

std::vector<std::uint8_t> scramble_name(std::vector<std::uint8_t> b, unsigned (*r)(int)) {
    for (std::size_t i = 0; i < b.size(); ++i) {  // [BYTES ISPACE.DLL seg10:0x4deb]
        if (static_cast<std::int8_t>(b[i]) < 1)
            b[i] = static_cast<std::uint8_t>(-static_cast<int>(r(static_cast<int>(i)) % 96 + 2));
        else
            b[i] = static_cast<std::uint8_t>(b[i] ^ kNameKey[i % 3]);
    }
    return b;
}

// ISPACE.DLL seg10:0x4c68(a, 0, b, 0, 0) and seg10:0x946a [BYTES ISPACE.DLL seg10:0x4d45
// seg10:0x4d5a seg10:0x4d63 seg10:0x4d77 seg10:0x4d82] [BYTES seg10:0x9482 seg10:0x948c].
int read_name(const std::vector<std::uint8_t>& record, bool found, std::string* name) {
    std::vector<std::uint8_t> b(record.begin(),
                                record.begin() + static_cast<std::ptrdiff_t>(
                                                     std::min(record.size(), kNameBytes)));
    b.resize(kNameBytes, 0);
    int result = found ? 1 : 0;
    // [DESIGN] A missing resource leaves the original's buffer (an uninitialised frame buffer
    // of seg10:0x885e) as it was; the port starts from zeros, which decode to an empty name.
    if (found) b = descramble_name(b);
    if (b[0] == 0 || b[1] == 0 || b[2] == 0) {
        result = 0;
        for (std::size_t i = 3; i < kNameBytes; ++i) b[i - 3] = b[i];  // bytes 3-49 to 0-46
    }
    b[0x30] = 0;  // seg10:0x948c
    if (name) {
        name->clear();
        for (std::uint8_t c : b) {
            if (c == 0) break;
            name->push_back(static_cast<char>(c));
        }
    }
    return result;
}

// seg10:0x885e [BYTES seg10:0x886d seg10:0x8874 seg10:0x8896 seg10:0x8899 seg10:0x88a0
// seg10:0x88a4 seg10:0x88ab seg10:0x8975 seg10:0x8983 seg10:0x8995 seg10:0x89a2 seg10:0x89aa
// seg10:0x89d0 seg10:0x8a26 seg10:0x8a3f seg10:0x8a52 seg10:0x8a87 seg10:0x8a91 seg10:0x8a97
// seg10:0x8aa0 seg10:0x8ab2 seg10:0x8ad0 seg10:0x8af3 seg10:0x8afc seg10:0x8b0a].
Edition edition_check(const Edition& prev, int arg, const EditionInputs& in, int* ret) {
    (void)arg;  // CDDrive counts as empty: the start-up call takes the same path (ADR 0008 d. 4)
    Edition e = prev;
    const auto done = [&](int r) {
        if (ret) *ret = r;
        return e;
    };
    if (prev.edition == -1) return done(1);  // returns at once, reading nothing

    std::int16_t dayval = w16(edition_day_value(in.tm_year, in.tm_yday));
    e.checked = 1;
    e.edition = 0;
    e.plus = 0;
    e.invalid = 0;
    // The DGROUP:0x5d0 branches (the CD flag local_172, DGROUP:0x920c = 3) are dead in
    // v1.29.08 [BYTES seg10:0x8959-0x89c7]: the local flag stays 0.
    const int r2 = record_check(in.install_record, in.vidsp, in.pdev);
    e.record_result = r2;
    if (r2 == 0) {
        e.edition = -1;
        if (read_name(in.name_record, in.name_found, &e.name) != 0) e.plus = 1;
        return done(0);
    }
    if (r2 > 0) {
        const std::int16_t v = w16(in.vidsp), s = w16(in.bit32sp), p = w16(in.pdev);
        const std::int16_t k = w16(w16(p - v) ^ 0xff);
        const std::int16_t dd = w16(dayval - k);
        e.day = w16(dd + 1);
        if (e.day < 1) e.day = 200;
        const std::int16_t x = w16(2 * v - p - 1231);  // - 0x4cf
        if (x == s) {
            e.day = w16(-1 - dd);
            if (dd >= 0 && dd < 35) {
                e.edition = 2;
                return done(0);
            }
        }
        if (dd >= 0 && dd < 14) {
            e.edition = 1;
            return done(0);
        }
    }
    if (r2 == 1000) e.invalid = 1;
    e.edition = 0;
    return done(1);
}

}  // namespace is::shell
