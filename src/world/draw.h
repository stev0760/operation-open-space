// The wave frame's own drawing (docs/re/world.md "Drawing", "Update loop"): the player's sprite
// seg7:0x1208, the icon dissolve seg7:0x3a42 and its loop in the frame, the rock cell from the
// rotation, the ship bitmap table DGROUP:0x39e8, and the frame's draw calls before the movers
// in address order (ADR 0013 decision 8). Every routine draws through world::Blitter on the
// window DC, which the surface stands for (ADR 0013 decision 1). Portable, no Win32.
#pragma once

#include <cstdint>

#include "formats/ships.h"
#include "world/records.h"
#include "world/sprite.h"

namespace is::world {

// The DGROUP words the frame's drawing reads beyond the sprite routine's.
struct DrawGlobals {
    SpriteGlobals sprite;               // seg8:0xb76's words (window DC, player, W, H, ...)
    std::int16_t state = 0;             // DGROUP:0x90f4, the screen state (1 = wave)
    std::int16_t dg_9130 = 0;           // DGROUP:0x9130: not 0 in the seg18 mode [OPEN]
    // DGROUP:0x3a38, slot 0x28 of the DC table DGROUP:0x39e8: ship record 0's image, so the
    // player's only when DGROUP:0x9646 = 0 (world.md "The player's sprite", "Conditions";
    // structs.md "Image slots"; M3 s3 fixer, C071) [BYTES seg7:0x1257 seg13:0xa9b].
    std::int16_t dg_3a38 = 0;
    std::int16_t last_effect = 0;       // DGROUP:0x9434: the last effect-table index in use
    std::int16_t detail = 0;            // DGROUP:0x9208 `Detail`
    std::uint8_t dissolve_step = 0;     // byte DGROUP:0x8ffe, the dissolve's decrement [OPEN]
    DcId strip_dc = kDcNone;            // DGROUP:0x3a02, the icon strip (world.md "Icons")
    Rect16 player_rect;                 // DGROUP:0x1e5c, written by seg7:0x1208
    std::int16_t dg_58a = 0;            // DGROUP:0x58a, cleared by seg7:0x1208 [OPEN]
};

// The routines the dissolve calls that belong to other parts of the wave; the defaults do
// what an absent routine does in a test (nothing, and 0 for seg13:0x59e8).
class DissolveHooks {
public:
    virtual ~DissolveHooks() = default;
    // seg13:0x59e8(0, i): an off-screen icon's dissolve returns at once when it gives 0
    // (world.md "Icon dissolve", "Off screen"; C081) [BYTES seg7:0x3a86 seg7:0x3a92]; what
    // seg13:0x59e8 tests is [OPEN].
    virtual bool seg13_59e8(int a, int i) { return (void)a, (void)i, false; }
    // seg7:0x65aa(0, i) and seg7:0x2080(0, -1) after the last step (world.md "Icon dissolve",
    // "End"; C083) [BYTES seg7:0x3b8d seg7:0x3b9d]; what they do is [OPEN].
    virtual void seg7_65aa(int a, int i) { (void)a, (void)i; }
    virtual void seg7_2080(int a, int b) { (void)a, (void)b; }
};

// Everything the drawing routines read and write. The records and globals are the wave's
// (world.h); the views here only borrow them.
struct DrawContext {
    Pools& pools;
    ShipTable& ships;
    const EffectTable& effects;
    ImageSlotTable& images;   // DGROUP:0x39e8: one DC word per image slot
    DrawGlobals& g;
    Blitter& out;
    ToScreen to_screen;       // seg8:0x29ea
    DissolveHooks* hooks = nullptr;
};

// ---- the player's sprite seg7:0x1208 [BYTES seg7:0x1208-0x13af]

// The player's cell origin in its sprite sheet (the frame choice of seg7:0x1208), 16-bit
// products: types 0 and 7 pick the column by heading, heading * (size + gap) + gap; the
// others take 16 * size + 17 * gap. Row: 2 * gap + size for types > 3; gap for types <= 3
// with ship byte +0xe9 set; the record's +0x12 unchanged otherwise (`keep_y`).
struct PlayerCell {
    std::int16_t src_x = 0, src_y = 0;
    bool keep_y = false;
};
PlayerCell player_cell(std::int16_t type, std::int16_t heading, std::int16_t size,
                       std::int16_t gap, std::uint8_t ship_be9);

// The conditions of seg7:0x1208: state 1, the player in play, DGROUP:0x951a, 0x9130 and
// 0x3a38, the special-wave skip, and the effect-table skip (an entry of kind 1..8 on the
// player's ship while ship byte +0xec is 0).
bool player_sprite_drawn(const DrawContext& c);

// seg7:0x1208: sets DGROUP:0x1e5c to the cell inset by 2, the cell origin, and draws with
// seg8:0xb76(0x0e00 + player, 1, DGROUP:0x39e8[slot + 0x28], 0); clears DGROUP:0x58a.
void draw_player_sprite(DrawContext& c);

// ---- rocks

// seg7:0x3d40-0x3d87: a rock on screen takes its frame from the rotation, +0x28 = +0x26 / 100
// (0..17), and its cell x +0x10 = (size + gap) * frame + gap, 16-bit. The rock mover
// (seg7:0x3bae) calls it before its draw.
void rock_cell(ObjectRecord& rock);

// ---- the icon dissolve seg7:0x3a42 [BYTES seg7:0x3a42-0x3bad]

// seg7:0x3a42(i): one dissolve step of icon i of pool 0. Size and gap are pool-0 record 0's.
void dissolve_icon(DrawContext& c, int i);
// seg7:0xbf2-0xc1a: the frame's loop, seg7:0x3a42(i) for every in-use pool-0 record whose
// counter byte +0x51 is not 0.
void dissolve_pass(DrawContext& c);

// ---- the frame's draw calls before the movers (ADR 0013 decision 8)

// seg8:0x158a(0) at seg7:0xb87, the stars: what it draws is [OPEN] (world.md Open questions,
// "Draw order"); the port draws nothing.
void draw_stars(DrawContext& c, int arg);
// seg12:0x3778 at seg7:0xbed, the lines: [OPEN] as the stars; the port draws nothing.
void draw_lines(DrawContext& c);

// The view, the visibility pass and the timer step are other parts' (world.h); the frame
// calls these in this order, seg7:0xb87-0xc1a:
//   draw_stars(c, 0)          seg7:0xb87  seg8:0x158a(0)          [OPEN] stub
//   view                      seg7:0xb8c  seg8:0x0
//   (seg7:0x1d0c when DGROUP:0x9140 != 0), visibility seg8:0x4e14 at seg7:0xba2
//   draw_lines(c)             seg7:0xbed  seg12:0x3778            [OPEN] stub
//   dissolve_pass(c)          seg7:0xbf2-0xc1a
// and the movers from seg7:0xcd3 draw their own objects (seg8:0xb76, seg7:0x1208).

// ---- the ship bitmap table DGROUP:0x39e8 [OPEN]

// What DGROUP:0x39e8[ship + 0x28] holds is [OPEN] (world.md Open questions, "Ship bitmaps").
// The port fills a ship's slot with its body's bitmap, IGRAPHIC.DLL #4000 BodyRecord::bitmap_id
// through seg10:0x6c02(id, 0, -1) [DESIGN ADR 0013] [OPEN]. A body whose bitmap is a file
// (kBodyBitmapFromFile) gets no DC (0) [OPEN].
DcId load_ship_body(Blitter& out, const fmt::BodyRecord& body);
// Puts ship `ship`'s DC in the slot seg7:0x1208 reads: ship_image_slot(ship, record).
void set_ship_image(ImageSlotTable& images, int ship, const ShipRecord& record, DcId dc);

}  // namespace is::world
