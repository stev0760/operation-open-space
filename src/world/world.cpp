// owner: world set-up (the single writer of this file in later stages).
// The World's helpers (world.h). Portable.
#include "world/world.h"

#include "world/player_sheet.h"

#include <cstring>
#include <utility>

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

namespace {
GuardTodo& guard_fn() {
    static GuardTodo f;
    return f;
}
}  // namespace

void set_guard_todo(GuardTodo f) { guard_fn() = std::move(f); }
void guard_todo(bool& once, const char* what, const char* address) {
    if (once) return;
    once = true;
    if (guard_fn()) guard_fn()(what, address);
}

std::int16_t word_at(const ShipRecord& s, int off) { return word_of(s, off); }
void set_word_at(ShipRecord& s, int off, int v) { set_word_of(s, off, v); }
std::int8_t sbyte_at(const ShipRecord& s, int off) {
    return static_cast<std::int8_t>(reinterpret_cast<const std::uint8_t*>(&s)[off]);
}
std::int8_t selected_weapon(const ShipRecord& s) {
    const int off = 0xb8 + s.weapon_slot;
    return off >= 0xb8 && off < static_cast<int>(sizeof(ShipRecord)) ? sbyte_at(s, off) : std::int8_t{-1};
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

const RosterRecord* roster_of(const World& w, int i) {
    if (i < 0 || i >= kShipCount) return nullptr;
    const int r = w.ships[static_cast<std::size_t>(i)].roster_index;
    if (r < 0 || r >= static_cast<int>(w.roster.size())) return nullptr;
    return &w.roster[static_cast<std::size_t>(r)];
}

std::int16_t rate_ship(World& w, int i) {
    if (i < 0 || i >= kShipCount || w.pools.records(w.roles.ships) <= i) return 0;
    ShipRecord& s = w.ship(i);
    const int n = s.design_index;
    const DesignRating d = n >= 0 && n < kShipCount ? w.design_ratings[static_cast<std::size_t>(n)]
                                                    : DesignRating{};
    s.kill_value = ship_rating(s, w.ship_object(i), d, roster_of(w, i));
    return s.kill_value;
}

void briefing_flame(World& w, int on, int i) {
    if (i < 0 || i >= kShipCount || w.pools.records(w.roles.ships) <= i) return;
    ObjectRecord& o = w.ship_object(i);
    o.flame = on != 0 ? static_cast<std::uint8_t>(idiv16(w.ship(i).thrust, 20)) : 0;
    if (sprite_links().flame_tail) sprite_links().flame_tail(on, i);
}

namespace {
std::function<std::shared_ptr<gfx::Surface>(int)> g_slot_images;
}  // namespace
void set_slot_images(std::function<std::shared_ptr<gfx::Surface>(int)> f) { g_slot_images = std::move(f); }
std::shared_ptr<gfx::Surface> slot_image(int slot) { return g_slot_images ? g_slot_images(slot) : nullptr; }
bool slot_images_linked() { return static_cast<bool>(g_slot_images); }

bool gate_post_live(const World& w, int rec) {
    if (rec < 0 || rec >= kPool8Max) return false;  // [DESIGN] storage bound
    const Pool8Entry& e = w.pool8[static_cast<std::size_t>(rec)];
    return e.at(0x1c) == 2 && e.at(0x0a) > 0;
}

}  // namespace is::world
