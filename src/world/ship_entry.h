// The other ships of a wave on screen (M5 stage 3 part B, track ENTRY; docs/re/ships.md
// "Pictures", "Entry of a wave's ships", "The entry buffer"): the sheet of every ship i in its own
// image slot 0x28 + i through the player's builder seg14:0x11a8, with its weapon images
// (seg14:0x13f3-0x1425), its flame strip rows 2i / 2i + 1 (seg10:0x5d3c(i)) and ship byte +0x5b,
// as seg14:0x0's tail loop makes them for each ship after seg16:0x3784(i, 1); and the gate
// placement of a ship that a call-in enters (seg13:0xae68). The entry of a wave's waiting ships
// itself is update.h ship_entry (seg7:0x1d0c), general in the ship index, and its buffer and
// effect entry_effect.h (seg12:0x2556, seg7:0x13b2, seg7:0x158e). Portable, no Win32.
#pragma once

#include <cstdint>
#include <string>

#include "world/player_sheet.h"

namespace is::gfx {
struct FlameHeadings;
}  // namespace is::gfx

namespace is::world {

class World;

// What seg14:0x1b84 stores in design record n for the sheet build and the flame [BYTES
// seg14:0x129c-0x12d0 seg14:0x1d55-0x1d69 seg14:0x1ed6-0x1f09 seg14:0x1c7f-0x1c88]: design word
// +0x36 (DGROUP:0x9708) = BOD +0x21, far +0x84 (DGROUP:0x9756) = BOD +0x23's file, bytes +0x18 /
// +0x1a (DGROUP:0x96ea / 0x96ec) = SHP +0x07 / +0x08, and the flame's words (FlameDesign) and
// BOD byte +0x19. The port has no 0xad-byte design table: the design fill (track ROSTER's
// RosterHost::design_fill, the program's) registers them per design index [DESIGN].
struct DesignSheetSource {
    bool known = false;
    int graphic_id = 0;
    std::string file;
    int colour_a = 0, colour_b = 0;
    FlameDesign flame;
    std::int8_t bod_19 = 0;
};
void set_design_sheet_source(int design, const DesignSheetSource& src);
const DesignSheetSource* design_sheet_source(int design);
void clear_design_sheet_sources();

// seg14:0x11a8(i)'s request for ship i from its design's source [BYTES seg14:0x11a8-0x12d0]: slot
// 0x28 + i, the graphic, the file, the scheme bytes, the row mode from ship bytes +0xe4 / +0xe9
// (sheet_mode), `sel` from INI `Quick`, the screen state and the frame counter (sheet_sel: the
// quick renderer ISPACE.DLL seg6:0x78ba for a computer ship built in a wave with `Quick` = 1),
// seg10:0x6ee6's scale flag and `Size`.
SheetRequest ship_sheet_request(const World& w, int i, const DesignSheetSource& src, int quick, int state);

// What the per-ship sheet step needs from the program.
struct PlaceSheetCtx {
    Blitter& out;
    ImageSlotTable& images;
    World& w;
    WeaponWords& words;
    SheetRenderer render;
    Anim4Renderer anim4;
    // seg10:0x7032's 16 headings of the flame for seg10:0x5d3c (null: no strip) and `Size`'s f.
    const gfx::FlameHeadings* flame = nullptr;
    int quick = 0;   // INI `Quick` DGROUP:0x931e
    int state = 0;   // the screen state DGROUP:0x90f4
    std::uint8_t new_body = 0;  // DGROUP:0x9563 (purchases.h Shop::new_body)
};
struct PlacedSheet {
    SheetBuild build;      // the build, or the slot's last build when kept
    bool rebuilt = false;  // seg16:0x3468(i, 3) ran seg14:0x11a8
    int weapon_sheets = 0; // seg10:0x533a slots filled
    // Design word +0x04 (DGROUP:0x96d6 + 0xad·n) from BOD +0x19 and the raw side, one more at Size
    // 0 for an odd side [BYTES seg14:0xbb5-0xbc2 seg14:0xc42-0xc68] (flame_radius_word); for the
    // caller's table (src/hud/wave_win32.cpp g_design_w04).
    bool has_w04 = false;
    std::int16_t w04 = 0;
};
// seg14:0x0's tail for ship i at the placement (the drawing side's part of seg16:0x3784(i, 1) ->
// seg16:0x3468(i, 3) -> seg14:0x11a8(i), then GetObject, +0x5b and seg10:0x5d3c(i)) [BYTES
// seg14:0xd1a-0xd32 seg16:0x3477-0x34a9 seg16:0x3621-0x3645 seg14:0x13f3-0x1425 seg14:0xd59
// seg14:0xc3e seg14:0xc7e]:
//  - `due` is seg14:0xd1a's test for ship i (roster.h purchase_due: the build flag of the
//    slot, wave 1, or a call-in) and the rebuild follows roster.h placement_rebuilds_sheet; a
//    rebuilt sheet sets object +0x3a = the slot's DC, writes the raw side and gap to roster bytes
//    +0x7a / +0x7b, or to object +0x22 / +0x24 for a ship without a record whose slot was empty
//    [BYTES seg14:0x12f5-0x134d], and becomes the slot's last build;
//  - a kept sheet (a ship that kept its roster index, its slot moved by take_ship_slot, or a later
//    wave without the flag) takes the slot's last build with the port's copy as drawn
//    (sheet_copy) and object +0x3a = the slot's DC;
//  - the weapon images seg10:0x533a of ship i's slots after every path [BYTES seg14:0x13f3-0x1425];
//  - with a rendered sheet: ship byte +0x5b from the sheet's height, design word +0x04, and
//    seg10:0x5c42 + seg10:0x5d3c(i) on the program's strip (player_sheet.h
//    ship_flame_after_sheet) when `flame` is set.
PlacedSheet place_ship_sheet(PlaceSheetCtx& c, int i, bool due);

// The two halves of place_ship_sheet in seg14:0x0's per-ship loop as roster_build.h splits it
// [BYTES seg14:0xd03-0xd9d seg14:0xb86-0xc7e]:
//  - RosterHost::purchases(w, i) is seg16:0x3784(i, 1), whose seg16:0x3468(i, 3) builds the
//    sheet: the program runs roster.h purchase_pass (and the computer shop) and then
//    ship_sheet_build(c, i, true), the rebuild under placement_rebuilds_sheet or the kept sheet,
//    and the weapon images of seg14:0x11a8's tail;
//  - RosterHost::ship_tail(w, i) runs roster_build.h ship_tail_sizes(w, i, ship_sheet_height(i))
//    (GetObject's biHeight of the slot's sheet, +0x5b and the `Size` step) and then
//    ship_sheet_flame(c, i): object +0x3a = the slot's DC [DESIGN: a kept ship whose purchases
//    did not run, its slot possibly moved, draws from its own slot], design word +0x04
//    (PlacedSheet::w04) and seg10:0x5c42 + seg10:0x5d3c(i) from the slot's sheet as drawn, on the
//    object's sizes after the `Size` step [BYTES seg14:0xc3e-0xc7e].
PlacedSheet ship_sheet_build(PlaceSheetCtx& c, int i, bool due);
std::int16_t ship_sheet_height(int i);
PlacedSheet ship_sheet_flame(PlaceSheetCtx& c, int i);

// The x of a ship that seg13:0xae68 enters at gate g [BYTES seg13:0xb0f2-0xb156
// seg13:0xb34f-0xb3ac seg13:0xb557-0xb5b1]: post 2g's x + W[g]/2 + post `size` - `size`/2, with
// W[g] the gate's word DGROUP:0x6682 + 0x1c·g and `size` object word +0x22 (pixels added to
// world units, as the original does), signed halvings toward zero, 16-bit. seg7:0x1d0c's own
// formula for a wave's waiting ships differs (update.cpp ship_entry).
std::int16_t callin_gate_x(std::int16_t post_x, std::int16_t post_size, std::int16_t size, std::int16_t width);

}  // namespace is::world
