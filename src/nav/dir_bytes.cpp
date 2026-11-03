// Navigation's directory-record fields in the record bytes (dir_bytes.h). Portable.
#include "nav/dir_bytes.h"

namespace is::nav {

namespace {
std::int16_t w16(const std::uint8_t* b, int off) { return static_cast<std::int16_t>(b[off] | (b[off + 1] << 8)); }
void put16(std::uint8_t* b, int off, int v) {
    b[off] = static_cast<std::uint8_t>(v & 0xff);
    b[off + 1] = static_cast<std::uint8_t>((static_cast<unsigned>(v) >> 8) & 0xff);
}
}  // namespace

DirRecord dir_from_bytes(const std::uint8_t* b) {
    DirRecord r;
    int n = 0;
    while (n < kDirPathBytes - 1 && b[kDirPath + n] != 0) ++n;
    r.path.assign(reinterpret_cast<const char*>(b + kDirPath), static_cast<std::size_t>(n));
    r.destroyed = w16(b, kDirDestroyed);
    r.captured = w16(b, kDirCaptured);
    r.state = b[kDirState];
    return r;
}

void dir_path_to_bytes(const std::string& path, std::uint8_t* b) {
    for (int i = 0; i < kDirPathBytes; ++i) b[kDirPath + i] = 0;
    const std::size_t n = path.size() < 109 ? path.size() : 109;
    for (std::size_t i = 0; i < n; ++i) b[kDirPath + static_cast<int>(i)] = static_cast<std::uint8_t>(path[i]);
}

void dir_to_bytes(const DirRecord& r, std::uint8_t* b) {
    dir_path_to_bytes(r.path, b);
    put16(b, kDirDestroyed, r.destroyed);
    put16(b, kDirCaptured, r.captured);
    b[kDirState] = r.state;
}

}  // namespace is::nav
