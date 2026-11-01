// The wave frame's own drawing (docs/re/world.md "Drawing", "Update loop"): the player's sprite
// seg7:0x1208, the icon dissolve seg7:0x3a42 and its loop in the frame, the rock cell from the
// rotation, the ship bitmap table DGROUP:0x39e8, and the frame's draw calls before the movers
// in address order (ADR 0013 decision 8). Every routine draws through world::Blitter on the
// window DC, which the surface stands for (ADR 0013 decision 1). Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>

#include "formats/ships.h"
#include "shell/rng.h"
#include "world/records.h"
#include "world/rock_images.h"
#include "world/sprite.h"
#include "world/world.h"

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

// ---- the stars seg8:0x158a and their set-up seg8:0x190e (s4-draw.md 2) (M3 s4 DRAW)

// The star count DGROUP:0x9154 of the INI read seg6:0xe36: Detail 0, 1, 2, 3 give 4, 7, 13,
// 18 (another Detail keeps `current`), one less when Bright != 0 [BYTES seg6:0xe36 seg6:0xe3b
// seg6:0xe49 seg6:0xe57 seg6:0xe65 seg6:0xe88 seg6:0xe94].
std::int16_t star_count_for(std::int16_t detail, std::int16_t bright, std::int16_t current);
constexpr int kStarMax = 18;

// seg8:0x190e, called at seg9:0x6117 in the wave start (before seg13:0x2dcc and the chance
// block): for each of the DGROUP:0x9152 stars six rand() in this order, x % W, y % H, a colour
// byte b % 255, a colour byte g % 255, the size % 3 + 1, the phase % 40; the drawn flag set
// [BYTES seg8:0x1924 seg8:0x192a seg8:0x1940 seg8:0x1948 seg8:0x1957 seg8:0x196e seg8:0x1982
// seg8:0x1994]. A W or H of 0 gives 0 for its coordinate, where the original faults [DESIGN].
void init_stars(World& w, shell::Rng& rng);

// The words seg8:0x158a reads beyond the World's.
struct StarInputs {
    DcId window_dc = kDcWindow;          // DGROUP:0x13dc
    std::int16_t ship_pool = kPoolShips; // DGROUP:0x9324
    std::int16_t player = 0;             // DGROUP:0x9646
    std::int16_t size = 1;               // DGROUP:0x9156 `Size`
    std::int16_t autopilot = 0;          // DGROUP:0x951a
    std::int16_t auto_vx = 0, auto_vy = 0;  // DGROUP:0x2394, 0x2396, the 0x951a motion [OPEN]
    std::int16_t shake = 0;              // DGROUP:0x238c
    std::int16_t shake_x = 0, shake_y = 0;  // DGROUP:0x2390, 0x2392
    std::int16_t demons_gate = 0;        // DGROUP:0x92e8
};
// seg8:0x1320, the Bright starfield, which seg8:0x158a calls instead while DGROUP:0x8ff2 != 0:
// not read [OPEN]; the default draws nothing.
using BrightStars = std::function<void(int arg)>;

// seg8:0x158a(arg) [BYTES seg8:0x158a-0x19ae]: the body runs on every second call (the
// toggle DGROUP:0x6ae; arg != 0 forces it); it zeroes the player's +0x0c, +0x0e when the
// player's `state` is 1 or byte +0x54 and DGROUP:0x92e8 are set; per star it blacks the old
// square, moves it against the player's velocity ((v << (Size + 1)) >> (6 - size), 16-bit),
// wraps it in DGROUP:0x1d30 with rand(), and PATCOPYs it with brush DGROUP:0x3b4c[n] outside
// DGROUP:0x1e5c; the side is min(size, 2 at Size 0 else 3).
void draw_stars(World& w, const StarInputs& in, const WaveGraphics& wg, Blitter& out,
                shell::Rng& rng, int arg, const BrightStars& bright = nullptr);
// The brush index n of a star's phase (0 ... 40): v = phase / 5, n = v + 10 below 4, else
// 18 - v [BYTES seg8:0x17b7 seg8:0x17cc seg8:0x17d7].
int star_brush(std::uint8_t phase);

// ---- the link lines seg12:0x3778 (s4-draw.md 2.5)

// The calls seg12:0x3778 makes outside the drawing; the defaults do nothing.
class LinkHooks {
public:
    virtual ~LinkHooks() = default;
    // seg12:0x7256(hdc, x0, y0, x1, y1) with BLACK_PEN (GetStockObject(7)) selected: the old
    // line's erase [OPEN: seg12:0x7256 is not read].
    virtual void line(DcId dc, std::int16_t x0, std::int16_t y0, std::int16_t x1, std::int16_t y1) {
        (void)dc, (void)x0, (void)y0, (void)x1, (void)y1;
    }
    virtual void seg7_2080(int a, int b) { (void)a, (void)b; }  // seg7:0x2080(10, -1) [OPEN]
};
// seg12:0x3778 [BYTES seg12:0x3778-0x390c]: each of the DGROUP:0x9300 (pool-10 allocated) slots
// with an end A breaks when an end's `state` is not 0 or an end is a ship (pool DGROUP:0x9324)
// whose byte +0xec is 0 or +0x147 is not 0: pool-10 record k's `state` = 1, A's +0x32 and B's
// +0x36 = -1, the slot's pools -1 and +0x08 0, the old line erased and the coordinates rolled.
// The placement leaves every slot without ends, so wave 1 draws nothing here.
void draw_lines(World& w, std::int16_t ship_pool, DcId window_dc, LinkHooks* hooks);

// The view, the visibility pass and the timer step are other parts' (world.h); the frame
// calls these in this order, seg7:0xb87-0xc1a:
//   draw_stars(..., 0)        seg7:0xb87  seg8:0x158a(0)
//   view                      seg7:0xb8c  seg8:0x0
//   (seg7:0x1d0c when DGROUP:0x9140 != 0), visibility seg8:0x4e14 at seg7:0xba2
//   draw_lines(...)           seg7:0xbed  seg12:0x3778
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
