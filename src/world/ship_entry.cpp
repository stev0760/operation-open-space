// The other ships of a wave on screen (ship_entry.h), 16-bit as the original (ADR 0013
// decision 7).
#include "world/ship_entry.h"

#include <array>

#include "world/records.h"
#include "world/roster.h"
#include "world/world.h"

namespace is::world {

namespace {

struct DesignSources {
    std::array<DesignSheetSource, kShipCount> src{};
};
DesignSources& sources() {
    static DesignSources s;
    return s;
}

// `cwd; sub ax, dx; sar ax, 1` [BYTES seg13:0xb132-0xb135].
std::int16_t half(std::int16_t v) { return idiv16(v, 2); }

}  // namespace

void set_design_sheet_source(int design, const DesignSheetSource& src) {
    if (design < 0 || design >= kShipCount) return;
    sources().src[static_cast<std::size_t>(design)] = src;
    sources().src[static_cast<std::size_t>(design)].known = true;
}

const DesignSheetSource* design_sheet_source(int design) {
    if (design < 0 || design >= kShipCount || !sources().src[static_cast<std::size_t>(design)].known) return nullptr;
    return &sources().src[static_cast<std::size_t>(design)];
}

void clear_design_sheet_sources() { sources() = DesignSources{}; }

SheetRequest ship_sheet_request(const World& w, int i, const DesignSheetSource& src, int quick, int state) {
    SheetRequest req;
    if (i < 0 || i >= kShipCount) return req;
    const ShipRecord& s = w.ships[static_cast<std::size_t>(i)];
    req.slot = kShipImageSlotBase + i;
    req.graphic_id = src.graphic_id;  // design +0x36 [BYTES seg14:0x12c4]
    req.file = src.file;              // design +0x84
    req.colour_a = src.colour_a;      // design byte +0x18 [BYTES seg14:0x12a9]
    req.colour_b = src.colour_b;      // design byte +0x1a
    req.mode = sheet_mode(s.has_flame, s.be9);  // [BYTES seg14:0x1245-0x1261]
    req.sel = sheet_sel(quick, state, w.frames, i, w.player);  // [BYTES seg14:0x1265-0x1297]
    req.scale = sheet_scale_flag(w.size, 0, req.sel);          // seg10:0x6ee6's w6 = 0
    req.size_level = w.size;
    return req;
}

PlacedSheet place_ship_sheet(PlaceSheetCtx& c, int i, bool due) {
    PlacedSheet r = ship_sheet_build(c, i, due);
    World& w = c.w;
    if (i < 0 || i >= kShipCount || w.pools.records(w.roles.ships) <= i) return r;
    if (r.build.rendered) {
        ShipRecord& s = w.ship(i);
        ObjectRecord& o = w.ship_object(i);
        s.anim_last = sheet_last_row(r.build.bm_height, o.size, o.gap);  // ship byte +0x5b [BYTES seg14:0xd59 seg14:0xc3e]
        const PlacedSheet f = ship_sheet_flame(c, i);
        r.has_w04 = f.has_w04;
        r.w04 = f.w04;
    }
    return r;
}

std::int16_t ship_sheet_height(int i) {
    // GetObject(DGROUP:0x3ad0 + 2·(0x28 + i)) -> biHeight [BYTES seg14:0xd59]: the slot's last
    // build [DESIGN].
    const SheetBuild* b = last_build(i);
    return b && b->rendered ? b->bm_height : std::int16_t{0};
}

PlacedSheet ship_sheet_flame(PlaceSheetCtx& c, int i) {
    PlacedSheet r;
    World& w = c.w;
    if (i < 0 || i >= kShipCount || w.pools.records(w.roles.ships) <= i) return r;
    ShipRecord& s = w.ship(i);
    ObjectRecord& o = w.ship_object(i);
    const int slot = kShipImageSlotBase + i;
    if (c.images[static_cast<std::size_t>(slot)] != kDcNone)
        o.w3a = static_cast<std::int16_t>(c.images[static_cast<std::size_t>(slot)]);  // [DESIGN]
    if (const SheetBuild* last = last_build(i)) r.build = *last;
    if (const SheetPixels* cp = sheet_copy(i)) r.build.pixels = *cp;
    const DesignSheetSource* src = design_sheet_source(s.design_index);
    if (!r.build.rendered || !src) return r;
    // Design word +0x04 from BOD +0x19 and the raw side [bp-0x24], one more at Size 0 for an odd
    // side [BYTES seg14:0xbb5-0xbc2 seg14:0xc42-0xc68].
    const std::int16_t side24 =
        (w.size == 0 && (r.build.side & 1) != 0) ? add16(r.build.side, 1) : r.build.side;
    r.w04 = flame_radius_word(src->bod_19, side24);
    r.has_w04 = true;
    if (c.flame) ship_flame_after_sheet(c.out, *c.flame, i, r.build, o, s, src->flame, w.size);  // [BYTES seg14:0xc7e]
    return r;
}

PlacedSheet ship_sheet_build(PlaceSheetCtx& c, int i, bool due) {
    PlacedSheet r;
    World& w = c.w;
    if (i < 0 || i >= kShipCount || w.pools.records(w.roles.ships) <= i) return r;
    ShipRecord& s = w.ship(i);
    ObjectRecord& o = w.ship_object(i);
    const DesignSheetSource* src = design_sheet_source(s.design_index);
    const int slot = kShipImageSlotBase + i;
    const bool has_dc = c.images[static_cast<std::size_t>(slot)] != kDcNone;
    const std::int16_t special = s.design_index >= 0 && s.design_index < kShipCount
                                     ? w.design_ratings[static_cast<std::size_t>(s.design_index)].special
                                     : std::int16_t{-1};
    SheetRequest req;
    if (src) {
        req = ship_sheet_request(w, i, *src, c.quick, c.state);
    } else if (const SheetRequest* kept = sheet_request(i)) {
        req = *kept;  // the slot's last request (a kept sheet) [DESIGN]
    } else {
        sheet_todo("ship " + std::to_string(i) + "'s design has no sheet source (seg14:0x1b84's design words)",
                   "seg14:0x11a8");
        return r;
    }
    if (src && placement_rebuilds_sheet(due, c.new_body, special, has_dc)) {
        r.build = rebuild_ship_sheet(c.out, c.images, s, o, i, req, c.render);  // seg16:0x3468(i, 3)
        r.rebuilt = true;
        if (r.build.rendered) {
            // [BYTES seg14:0x12f5-0x134d]: the raw side and gap to the object for a ship without a
            // record whose slot was empty, else to the roster bytes +0x7a / +0x7b.
            if (r.build.to_object) {
                o.size = r.build.side;
                o.gap = r.build.gap;
            } else if (s.roster_index >= 0 && static_cast<std::size_t>(s.roster_index) < w.roster.size()) {
                RosterRecord& rr = w.roster[static_cast<std::size_t>(s.roster_index)];
                rr.size = static_cast<std::uint8_t>(r.build.side);
                rr.gap = static_cast<std::uint8_t>(r.build.gap);
            }
            set_last_build(i, r.build);
        }
    } else {
        // The sheet still in the slot [BYTES seg16:0x3477-0x34a9]: its last build with the copy as
        // drawn (flame pastes and wing cuts included), and object +0x3a = the slot's DC (the
        // set-up's purchase pass stores it, seg16:0x3645) [DESIGN].
        if (const SheetBuild* last = last_build(i)) r.build = *last;
        if (const SheetPixels* cp = sheet_copy(i)) r.build.pixels = *cp;
        o.w3a = static_cast<std::int16_t>(c.images[static_cast<std::size_t>(slot)]);
    }
    // seg14:0x11a8's tail [BYTES seg14:0x13f3-0x1425], after every path as the player's
    // placement runs it (src/hud/wave_win32.cpp build_player_sheet) [DESIGN].
    r.weapon_sheets = build_weapon_sheets(c.out, c.images, w.pools, c.words, s,
                                          weapon_sheet_sel(c.quick, c.state, w.frames), w.size, c.render, c.anim4);
    return r;
}

std::int16_t callin_gate_x(std::int16_t post_x, std::int16_t post_size, std::int16_t size, std::int16_t width) {
    // ax = half(size) - post size; neg; + half(W) [BYTES seg13:0xb12c-0xb156].
    return add16(post_x, add16(sub16(post_size, half(size)), half(width)));
}

}  // namespace is::world
