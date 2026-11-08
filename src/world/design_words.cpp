// The design words of the sheet build and the wing tear (src/world/design_words.h).
#include "world/design_words.h"

#include <array>

namespace is::world {

namespace {

constexpr int kDesigns = 8;  // kShipCount: one design per ship slot [DESIGN]

std::int16_t w16(long v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff)); }

// cwd; sub ax, dx; sar ax, 1
std::int16_t half16(std::int16_t v) { return static_cast<std::int16_t>((v - (v < 0 ? -1 : 0)) >> 1); }

struct Store {
    std::array<DesignWordSet, kDesigns> words{};
    std::array<DesignBody, kDesigns> bodies{};
};

Store& store() {
    static Store s;
    return s;
}

bool ok(int n) { return n >= 0 && n < kDesigns; }

}  // namespace

const DesignWordSet& design_words(int n) {
    static const DesignWordSet none{};
    return ok(n) ? store().words[static_cast<std::size_t>(n)] : none;
}

void set_design_words(int n, const DesignWordSet& d) {
    if (ok(n)) store().words[static_cast<std::size_t>(n)] = d;
}

void reset_design_words() { store() = Store{}; }

const DesignBody& design_body(int n) {
    static const DesignBody none{};
    return ok(n) ? store().bodies[static_cast<std::size_t>(n)] : none;
}

void set_design_body(int n, std::int8_t b1a, std::int8_t b1e, std::int8_t b1f) {
    if (ok(n)) store().bodies[static_cast<std::size_t>(n)] = DesignBody{b1a, b1e, b1f, true};
}

std::int16_t design_half_side(std::int16_t side, int size_level) {
    // inc [bp-0x24] at Size 0 for an odd side [BYTES seg14:0xbb5-0xbc2]
    const std::int16_t s = (size_level == 0 && (side & 1) != 0) ? w16(static_cast<long>(side) + 1) : side;
    return half16(s);  // [BYTES seg14:0xc42-0xc48]
}

std::int16_t design_band(std::int8_t bod_1a, int size_level) {
    const std::int16_t v = static_cast<std::int16_t>(bod_1a);              // cbw [BYTES seg14:0x2070]
    const std::int16_t sh = w16(static_cast<long>(v) << (size_level & 0x1f));  // shl ax, cl [BYTES seg14:0x207e]
    return half16(static_cast<std::int16_t>(sh >> 1));                        // sar 1; cwd; sub; sar 1 [BYTES seg14:0x2080-0x208b]
}

void design_band_recompute(int n, int size_level) {
    if (!ok(n)) return;
    const DesignBody& b = store().bodies[static_cast<std::size_t>(n)];
    if (!b.known) return;
    store().words[static_cast<std::size_t>(n)].w3c = design_band(b.b1a, size_level);  // [BYTES seg8:0x2d6f-0x2d8d]
}

void design_barrels(DesignWordSet& d, std::int8_t b1e, std::int8_t b1f, std::int16_t side, int size_level) {
    const std::int16_t c = half16(side);  // [BYTES seg14:0x1387-0x138d]
    const std::int16_t sz = static_cast<std::int16_t>(size_level);  // DGROUP:0x9156
    d.w68 = w16(static_cast<long>(b1e) - c - sz);  // [BYTES seg14:0x13a2-0x13b2]
    d.w6a = w16(static_cast<long>(b1f) - c);       // [BYTES seg14:0x13c3-0x13ca]
    d.w6c = w16(-(static_cast<long>(d.w68) + sz));  // [BYTES seg14:0x13d2-0x13d8]
    d.w6e = d.w6a;                                  // [BYTES seg14:0x13e0]
}

}  // namespace is::world
