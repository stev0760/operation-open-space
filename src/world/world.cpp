// The World's helpers (world.h). Portable.
#include "world/world.h"

#include <cstring>

namespace is::world {

namespace {
template <typename T>
std::int16_t word_of(const T& rec, int off) {
    std::uint8_t b[2];
    std::memcpy(b, reinterpret_cast<const std::uint8_t*>(&rec) + off, 2);
    return static_cast<std::int16_t>(b[0] | (b[1] << 8));
}
template <typename T>
void set_word_of(T& rec, int off, int v) {
    const std::uint8_t b[2] = {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(static_cast<unsigned>(v) >> 8)};
    std::memcpy(reinterpret_cast<std::uint8_t*>(&rec) + off, b, 2);
}
}  // namespace

std::int16_t word_at(const ShipRecord& s, int off) { return word_of(s, off); }
void set_word_at(ShipRecord& s, int off, int v) { set_word_of(s, off, v); }
std::int8_t sbyte_at(const ShipRecord& s, int off) {
    return static_cast<std::int8_t>(reinterpret_cast<const std::uint8_t*>(&s)[off]);
}
void set_byte_at(ShipRecord& s, int off, int v) {
    reinterpret_cast<std::uint8_t*>(&s)[off] = static_cast<std::uint8_t>(v);
}
std::int16_t word_at(const ObjectRecord& r, int off) { return word_of(r, off); }
void set_word_at(ObjectRecord& r, int off, int v) { set_word_of(r, off, v); }

std::int16_t distance_cells(std::int16_t x1, std::int16_t y1, std::int16_t x2, std::int16_t y2,
                            std::int16_t& dx, std::int16_t& dy, std::int16_t cells_x,
                            std::int16_t cells_y) {
    // seg8:0x285a, the DGROUP:0x9130 = 0 path [BYTES seg8:0x285a].
    auto axis = [](std::int16_t a, std::int16_t b, std::int16_t cells) {
        std::int16_t d = sub16(sar16(a, 3), sar16(b, 3));
        if (sar16(cells, 1) < (d < 0 ? -d : d)) {
            const std::int16_t move = d >= 0 ? static_cast<std::int16_t>(-cells) : cells;
            d = sub16(add16(sar16(a, 3), move), sar16(b, 3));
        }
        return d;
    };
    dx = axis(x1, x2, cells_x);
    dy = axis(y1, y2, cells_y);
    const std::int32_t a = dx < 0 ? -dx : dx;
    const std::int32_t b = dy < 0 ? -dy : dy;
    const std::int32_t e = a <= b ? (a >> 1) + b : (b >> 1) + a;
    if (e == 0) return 1;
    return lo16(((a * a + b * b) / e + e) >> 1);
}

}  // namespace is::world
