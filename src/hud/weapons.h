// The weapon table DGROUP:0x2f54 (seg10:0x1fb2, called from the state-1 entry seg10:0x51) and
// what the weapon display seg11:0x238e (id 0xdc) reads from it. Portable.
//
// seg10:0x1fb2 writes 28 blocks of 0x50 bytes from DGROUP:0x2f54 (the last, DGROUP:0x37c4, with
// name word -1); 23 words differ when DGROUP:0x920c (the edition, shell.md "Editions") is 3
// [BYTES seg10:0x1fb2-0x2ff8]. The words +0x48 ... +0x4e of block w are words -8 ... -2 of
// record w + 1: the picture routine seg11:0x2ff6(w) reads DGROUP:0x2f4c/0x2f4e/0x2f52 + 0x50·w
// [BYTES seg11:0x2ff6]. Record fields the HUD reads: +0x00 the ENG_LANG name string, +0x0a
// counted (non-zero: the line shows the count), +0x2e (non-zero: the count is not shown).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "hud/canvas.h"
#include "hud/host.h"
#include "world/records.h"
#include "world/sprite.h"

namespace is::hud {

constexpr int kWeaponBlocks = 28;        // DGROUP:0x2f54 ... DGROUP:0x3813
constexpr int kWeaponBlockSize = 0x50;   // bytes per record
constexpr std::uint16_t kDgWeaponTable = 0x2f54;

class WeaponTable {
public:
    using Block = std::array<std::int16_t, kWeaponBlockSize / 2>;
    // seg10:0x1fb2 with DGROUP:0x920c = `edition`.
    explicit WeaponTable(int edition = 0);
    // The signed word at DGROUP:0x2f54 + 0x50·w + offset (offset may be negative).
    std::int16_t word(int w, int offset) const;
    const Block& block(int w) const { return blocks_[static_cast<std::size_t>(w)]; }

private:
    std::array<Block, kWeaponBlocks> blocks_{};
};

// The weapon display's view of weapon w (the signed ship byte +0xb8 + slot) with count `count`
// (the ship byte +0xae + slot) [BYTES seg11:0x2a60-0x2ad1].
WeaponView weapon_view(const WeaponTable& t, int weapon, int count);

// seg11:0x2ff6(w), the weapon picture DGROUP:0x5040 of seg11:0x2f8c, as a plan
// (work/analysis/m4/s2-display.md section 1, verified claims 1-10) [BYTES seg11:0x2ff6-0x3394]:
// - none for w = 0 [BYTES seg11:0x3003-0x300b];
// - the graphic word +0x02 through seg10:0x6c02(id, 0, -1) when +0x0a = 0 or word -2 = -1, and
//   w != 0x18 [BYTES seg11:0x300e-0x303d];
// - otherwise composed (compose_weapon_picture): frame 4 of the weapon's sheet in image slot
//   W(w)-4 (slot 0x28 + player for w = 0x18), stretched to a square of side S, with an overlay
//   graphic chosen by W(w)+0x14 at its right.
struct WeaponPictureInputs {
    int size_level = 1;   // byte DGROUP:0x9156, `Size`
    int player = 0;       // DGROUP:0x9646
    // W(w)-4, the run-time image slot seg10:0x533a writes (the const table holds -1 there):
    // -1 reads the table's word [OPEN: the mutable word is MISSILE's seg10:0x533a].
    int sheet_slot = -1;
    // w = 0x18 with W(w)+0x0a = 0: the sign-extended roster bytes +0x7a / +0x7b of the roster
    // record DGROUP:0x6e24 + 0x7c·DGROUP:0x931c [BYTES seg11:0x3058-0x3078] (the ship sheet's
    // side and gap seg14:0x11a8 writes there).
    int roster_7a = 0, roster_7b = 0;
};
struct WeaponPicture {
    enum class Kind { kNone, kGraphic, kComposed };
    Kind kind = Kind::kNone;
    int graphic = 0;  // kGraphic: the IGRAPHIC id
    // ---- kComposed
    int weapon = 0;
    int name = 0;               // W(w)+0x00 [BYTES seg11:0x317d seg11:0x3222]
    // The overlay graphic G by W(w)+0x14: 1, 4 -> 0xbf; 2 -> 0xc4; 3 -> 0xc5; 0 -> 0xbd for
    // w = 0x18, else none (0) [BYTES seg11:0x307b-0x30cb].
    int overlay = 0;
    // W(w)+0x14 outside 0 ... 4: the original's handle is the uninitialised [bp-0xe]
    // (s2-display-verify.md claim 3); the port takes none [OPEN] (no table value reaches it).
    bool overlay_unset = false;
    int sheet_slot = -1;        // DGROUP:0x39e8 index: W(w)-4, or 0x28 + player for w = 0x18
    int a_raw = 0;              // A before the odd rule: W(w)-8 or roster +0x7a
    int side_plain = 0;         // without an overlay: (W(w)-8 << 2) / 3 or 2·(W(w)-8)
    int side_overlay = 0;       // with an overlay: A (w = 0x18) or 2·A
    int cell = 0, gap = 0;      // A and B after the odd rule and the Size scaling
    int sx = 0, sy = 0;         // the source cell in the sheet
};
WeaponPicture weapon_picture(const WeaponTable& t, int weapon,
                             const WeaponPictureInputs& in = WeaponPictureInputs{});

// seg11:0x30d0-0x338b on the canvas: Img::kWeapon becomes the composed bitmap. The overlay
// graphic G, when the caller loaded it, is in Img::kWeaponOverlay (absent: G = 0, the original's
// handle when seg10:0x6c02 fails); the sheet DC DGROUP:0x39e8[slot] is Img::kWeaponSheet. The
// caller drops both afterwards (DeleteObject(G), the slot's DC stays).
void compose_weapon_picture(Canvas& c, const WeaponPicture& p);

// ---- the selection: seg11:0xadf0 (Tab) and seg11:0xae94 (the wave start)
struct TabSelection {
    int slot = 0;            // ship word +0x18
    int dg_9524 = 0;         // DGROUP:0x9524
    bool clear_b50 = false;  // the player's pool-14 object byte +0x50 = 0
};
// seg11:0xadf0 [BYTES seg11:0xadf0-0xae93]: `slot` is ship word +0x18, `slots` the signed ship
// byte +0xcb, `weapon_byte(j)` the ship byte +0xb8 + j (j may pass the ten slots: the original
// reads on into the record), `play_mode` DGROUP:0x9134.
TabSelection tab_select(int slot, int slots, const std::function<int(int)>& weapon_byte,
                        int play_mode);
// seg11:0xae94 [BYTES seg11:0xae9e-0xaec0]: the slot at the wave start, or `slot` unchanged when
// DGROUP:0x92e4 (`restored`) != 0.
int wave_start_slot(int slot, int slots, int restored);

// ---- the weapon keys of seg11:0x6b64 (s2-missile.md section 1, s2-display-verify.md claim 23)
enum class WeaponKey {
    kNone,    // not a weapon key here: the message goes on (ESC's WM_KEYUP among them)
    kTab,     // WM_KEYDOWN VK_TAB: seg11:0xadf0 then seg11:0x2f8c [BYTES seg11:0x6e50 seg11:0x71e3]
    kFire,    // seg11:0xac86, the weapon system [BYTES seg11:0x6e11 seg11:0x6f6e]
    kIgnored  // a weapon key the switch takes and drops (the gate, the demo, Strategy mode)
};
struct WeaponKeyState {
    bool gated = false;   // the player's dword +0x84 > now or object `state` != 0 [BYTES seg11:0x6c9b-0x6cd1]
    int demo = 1;         // DGROUP:0x95bc: 0 in the demo
    int play_mode = 0;    // DGROUP:0x9134
    int weapon = 0;       // the selected weapon byte +0xb8 + slot
};
// The decision for WM_KEYDOWN (0x100) and WM_KEYUP (0x101) with the remapped key: WM_KEYUP
// VK_RETURN / 'Z' fire in Action mode [BYTES seg11:0x6d19-0x6d56 seg11:0x6e11]; WM_KEYDOWN
// VK_RETURN / 'Z' fire only for weapon 13 (WILDCARD) in Action mode [BYTES seg11:0x6f58-0x6f6e];
// WM_KEYDOWN VK_TAB selects with no PlayMode test. All behind the gate, and the demo
// (DGROUP:0x95bc = 0) takes none of them [BYTES seg11:0x6d1e seg11:0x6e19-0x6e3e].
WeaponKey weapon_key(std::uint32_t msg, std::uint16_t key, const WeaponKeyState& s);

// ---- the run-time copy of the table (world/missile.h MissileState::words)

// seg10:0x1fb2 for the edition DGROUP:0x920c (World::w920c) into the words the wave reads and
// writes, copied in whole when the edition differs from the one `filled_for` names, so
// seg10:0x533a's run-time W(w)-4 stays between the sheet builds [DESIGN: one mutable copy, M4
// s2 fixer]. `filled_for` is empty until the first copy: every edition value, the registered
// edition's -1 among them (shell.md "Editions"), is a real edition and must copy once (M4 s5:
// a cache whose start value was -1 kept the registered copy's words all 0, so its SKY TALON
// launched nothing). Returns true when it copied.
bool fill_weapon_words(int edition, world::WeaponWords& words, std::optional<int>& filled_for);

// ---- seg10:0x4412(k, pool, id, n), the animation-4 sheet (weapons 10, 16 and 17)
// [BYTES seg10:0x4412-0x4648] (work/analysis/m4/s5-weapons.md 2.5, verified claims 14-18):
// seg10:0x533a calls it for a weapon whose W(w)+0x06 = 4 with (k, 6, W(w)-2, W(w)+0x26)
// [BYTES seg10:0x5449 seg10:0x5452 seg10:0x545e]. The graphic is taken as h x h, h its bmHeight
// (GetObject at seg10:0x443a; the width is never read). Frame i of n is the graphic shrunk to
// side h + 2(i - n), top-left aligned at (gap + i(gap + h), gap): the frames grow toward the
// last, which the weapon picture shows (hud/weapons.cpp weapon_picture, W+0x06 = 4).
struct Anim4Frame {
    int dx = 0, dy = 0, side = 0;
};
struct Anim4Sheet {
    int gap = 0;           // 5 for an odd h, 6 for an even h [BYTES seg10:0x4445-0x4465]
    int width = 0;         // lo16(lo16(h n) + lo16(gap (n + 1))) [BYTES seg10:0x4483-0x44a1]
    int height = 0;        // 2 gap + h [BYTES seg10:0x44c7]
    bool stretch = false;  // n != 1: StretchBlt in COLORONCOLOR, else one BitBlt [BYTES seg10:0x4540]
    std::vector<Anim4Frame> frames;
};
Anim4Sheet anim4_sheet_plan(int h, int n);

// seg10:0x4412's drawing on the Blitter from CreateCompatibleBitmap to the last frame
// [BYTES seg10:0x44aa-0x45f5]: a new image of width x height (its DC in DGROUP:0x39e8[k], the
// bitmap DGROUP:0x3ad0[k]), PatBlt BLACKNESS over it, then the n = 1 BitBlt SRCCOPY of the h x h
// graphic to (gap, gap), or SetStretchBltMode(3) and per frame StretchBlt SRCCOPY (seg10:0x9962
// with positive extents is a plain StretchBlt, ispace.md "seg10:0x9962"; a side <= 0 would take
// its mirror path, which no shipped weapon reaches [OPEN]). The GDI picks the rows and columns a
// COLORONCOLOR shrink keeps (Wine's in the oracle) [DESIGN ADR 0013 decision 1]. `graphic` is the
// DC of seg10:0x6c02(id, 3, -1); the caller loads it, deletes it after (seg10:0x45fd-0x4626) and
// writes `gap` into the pool's record 0 +0x24 (anim4_gap_record). Returns the sheet's DC.
world::DcId render_anim4_sheet(world::Blitter& out, world::DcId graphic, const Anim4Sheet& plan);
// seg10:0x4412's word store: es:[DGROUP:0x1df8[pool] + 0x24] = gap, record 0 of the pool
// [BYTES seg10:0x4449-0x4465]. Nothing when the pool has no record.
void anim4_gap_record(world::Pools& pools, int pool, int gap);

}  // namespace is::hud
