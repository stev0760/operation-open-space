// owner: M4 stage 6, track PAD.
// The launch pictures on the port's index images, shared by the desk (src/desk/gdi_desk.cpp,
// states 0 and 6) and the visit (src/hud/visit_win32.cpp): the pad bitmap DGROUP:0x92a8 of
// seg15:0x482e, the pad ship of seg15:0x2a08 (the desk's, and the visit's after a new body) and of
// seg15:0x20ca (the visit's), the thrust flame DGROUP:0x3cc8 of seg10:0x7032(1, 1, row) and the
// checkerboard shadow of ROP 0x00A803A9 (work/analysis/m4/s6-pad.md sections 1, 2 and 6 with
// s6-pad-verify.md, claims 30 and 32 as CORRECTED). Every bitmap is an index image of Gfx (ADR
// 0007 decision 2); the GDI calls, ROPs and extents are the bytes'. The port's device is 8 bpp
// (DGROUP:0x90ee = gfx::kBitsPixel, ADR 0007 decision 4), so k = 2, DGROUP:0x90f0 is set (the
// flame through seg10:0x9962) and every depth <= 8 shadow is drawn (ruling R2 of M4 stage 6, ADR
// 0007 Consequences). The records and tables are portable; the drawing is Win32 only.
#pragma once

#include <array>
#include <functional>
#include <string>

#include "formats/dib.h"
#include "gfx/gfx.h"
#include "gfx/pad.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace is::gfx {

using LaunchTodo = std::function<void(const std::string&, const char*)>;

// k of seg15:0x2a08, seg15:0x20ca, seg15:0x1a42 and seg15:0x4c76: 4 when DGROUP:0x90ee < 8, else 2
// [BYTES seg15:0x2a17-0x2a25 seg15:0x1a4f-0x1a5d].
constexpr int kLaunchK = kBitsPixel < 8 ? 4 : 2;

#ifdef _WIN32
// The pad bitmap DGROUP:0x92a8 (pad.h pad_picture) as an index image: each grey through
// `index` (the caller's PALETTERGB resolution on the palette of the moment), black 0.
ImagePtr pad_image(Gfx& g, const PadPicture& p, const std::function<int(int grey)>& index);

// DGROUP:0x5eaa, the checkerboard pattern brush: CreateBitmap(8, 8, 1, 1) of the rows at
// DGROUP:0x6b6 (0xaa, 0x55 four times, each byte then 0x00; world/rock_images.h kCheckerRows) and
// CreatePatternBrush (shell.md "Pattern brushes") [BYTES DGROUP:0x6b6 seg10:0xe4 seg10:0x10a]; the
// caller deletes it. On an index image the brush's 0 bits take the DC's text colour (index 0) and
// its 1 bits the background (index 255) [REF Win32 SDK CreatePatternBrush].
HBRUSH checker_brush();
// The shadow: SelectObject(dst, DGROUP:0x5eaa) and, at DGROUP:0x90ee <= 8, StretchBlt(dst, x, y, w,
// h, mask, sx, sy, sw, sh, 0x00A803A9) (seg10:0x9962) or BitBlt when sw < 0, then the brush
// deselected. ROP 0xA8 is D AND (P OR S): outside the ship (mask 1, white) D stays, inside it D AND
// the checker [REF Win16 SDK ternary raster operations]. DGROUP:0x5ea8 below 8 bpp is not the
// port's [DESIGN ADR 0007 decision 4].
void shadow_blit(HDC dst, int x, int y, int w, int h, HDC mask, int sx, int sy, int sw = -1, int sh = -1);

// seg10:0x7032(1, 1, row), the flame DGROUP:0x3cc8 with its mask DGROUP:0x3cca (seg10:0x6ec6),
// `Size` forced to 1 [BYTES seg15:0x2bd0-0x2bf9 seg15:0x21ca] (verified claims 21-22): ordinal 18
// with x = 1, n = 5, y = row, then seg10:0x6308 when DGROUP:0x9130 = 0, no mirroring. Of its x = 1
// path the port has what s6-pad.md 2.3 reads: the load seg5:0x1b8e(hPal, 0x118, DGROUP:0x15f, 0,
// 4, -1, 0), direction 0's cell (no turn) with the gap fill and no 2 x 2 average, 18 x 18, the cell
// seg10:0x9962 reads from (0, sy, 18, 18); seg5:0x23a4's bitmap, the effect of DGROUP:0x1ea6 (y =
// row != 0) and the tests of seg5:0x2d88, seg5:0x2dba and seg5:0x4ea8 are unread [OPEN: ORD18-x1,
// ISPACE.DLL seg6:0x641c]: the port maps each colour to its nearest entry [DESIGN]. Null images
// when graphic 280 is not a 4-bpp picture.
struct LaunchFlame {
    ImagePtr img, mask;  // 18 x 18
};
LaunchFlame launch_flame(Gfx& g, const fmt::Dib& flame280, int row);
#endif

// What seg15:0x2a08's branch A reads (s6-pad.md 2.1): the MASTER.SHP record of INI MainShip (or
// the zeroed scratch record under ADR 0016 decision 2, ruling R4: body 0, a = b = 0, +0x09 = 0) and
// its MASTER.BOD record, the roster's wing bytes when the game is kept.
struct LaunchShipArt {
    fmt::Dib body;       // IGRAPHIC.DLL #4000 BOD +0x21 as stored
    int id = 0;          // BOD +0x21 [BYTES seg15:0x2b15]
    int a = 0, b = 0;    // SHP +0x07, +0x08 [BYTES seg15:0x2b2a-0x2b44]
    int fy = 0;          // sext BOD +0x19, the flame's y [BYTES seg15:0x2abf-0x2ac4]
    int band = 0;        // sext BOD +0x1a [BYTES seg15:0x2b5c-0x2b72]
    int body_flags = 0;  // BOD +0x20: 0x01 no thrust, 0x04 Pull-off Wings, 0x08 thrust behind
    int ship_flags = 0;  // SHP +0x09: bit 0 clear selects flame row 1 [BYTES seg15:0x2ae2-0x2af0]
    fmt::Dib flame;      // IGRAPHIC.DLL #4000 280 as stored (ordinal 18's 0x118)
    bool keep = false;   // DGROUP:0x92e4, a kept or restored game
    // Roster DGROUP:0x6e24 + 0x7c DGROUP:0x931c bytes +0x72 and +0x73: the wings still on.
    bool left_wing = true, right_wing = true;
};

// The pad ship's pictures: DGROUP:0x92b0 (ordinal 12's 2H x 2H body image), DGROUP:0x92b2 (the k s
// square with the flame), DGROUP:0x92b4 (seg15:0x20ca's flameless square; null after seg15:0x2a08,
// which deletes it [BYTES seg15:0x2c8d-0x2c9d]) and s = trunc(bmHeight(DGROUP:0x92b0) / k), or
// seg15:0x20ca's roster size sz.
struct LaunchShip {
    ImagePtr body, square, flameless;
    int s = 0;
    bool ok() const { return body && square; }
};
#ifdef _WIN32

// seg15:0x2a08's branch A and its common part [BYTES seg15:0x2a86-0x2bf9 seg15:0x2c88-0x302c]
// (verified claims 17-30): DGROUP:0x92b0 = seg10:0x73a6(id, BOD +0x23, -2, -2, 2, 0, 1, a, b)
// (smooth_render.h smooth_for and smooth_image); s = trunc(bmHeight / k), w8 = half(s - band);
// DGROUP:0x92b2 = CreateCompatibleBitmap(lo16(k s), lo16(k s)); the flame; then BitBlt(Q, 0, 0, k s,
// k s, B, 0, 0, SRCCOPY); with the thrust on (BOD +0x20 bit 0 clear): fx = half(s - 9), behind ->
// PatBlt(Q, BLACKNESS), seg10:0x9962(Q, fx k, fy k, 9k, 9k, FM, 0, sy, 18, 18, SRCAND) and of F
// SRCPAINT (sy = 0x12 only when k = 4 and the row is set), behind -> the body over it through its
// mask; the wing cuts when DGROUP:0x92e4 != 0 and Pull-off Wings: a missing left wing blacks columns
// 0 ... k w8 - 1, a missing right one k (s - w8) ... of both DGROUP:0x92b2 and DGROUP:0x92b0. The
// early return (state != 0 without a wing repair or a new body) is the caller's. `ok()` false when
// ordinal 12 returns nothing (a scheme byte 0xff, an unknown palette).
LaunchShip ship_2a08(Gfx& g, const LaunchShipArt& art, const LaunchTodo& todo);
#endif

// What seg15:0x20ca reads for the player's ship (s6-pad.md 2.2), d = 0xad times the design index
// word DGROUP:0x70de + 0x15a DGROUP:0x9646, S the ship record, R its roster record.
struct VisitShipArt {
    fmt::Dib body;          // IGRAPHIC.DLL #4000 of design word DGROUP:0x9708 + d
    int id = 0;             // word DGROUP:0x9708 + d (BOD +0x21)
    int a = 0, b = 0;       // bytes DGROUP:0x96ea + d, DGROUP:0x96ec + d (SHP +0x07, +0x08)
    int flame_row = 0;      // word DGROUP:0x9716 + d (design word +0x44)
    int behind = 0;         // word DGROUP:0x971a + d
    int ability = -1;       // word DGROUP:0x96fa + d; 3 = Pull-off Wings
    bool left_wing = true;  // ship byte +0xac (DGROUP:0x713c) != 0
    bool right_wing = true; // ship byte +0xad (DGROUP:0x713d) != 0
    bool flame_on = true;   // ship byte +0xe4 (DGROUP:0x7174) != 0
    int r7a = 0, r7b = 0;   // sext roster bytes +0x7a, +0x7b
    int fy = 0;             // sext BOD +0x19 of R's SHP's body
    int band = 0;           // sext BOD +0x1a of the same
    bool dead = false;      // DGROUP:0x9e5b = 0: seg15:0x28b2 [OPEN]
    int detail = 3;         // DGROUP:0x9208 `Detail`
    fmt::Dib flame;         // IGRAPHIC.DLL #4000 280 as stored
};
// sz = sext(R +0x7a) + half(sext(R +0x7b) - 6) [BYTES seg15:0x210b-0x2134].
int visit_ship_size(const VisitShipArt& v);

// seg15:0x20ca in states 6 and 8 (state 9's vertical flips belong to the Inner Demon's return, M6)
// [BYTES seg15:0x20d9-0x28a7] (verified claims 32-37): DGROUP:0x92b0 from the design's copies;
// DGROUP:0x92b2 and DGROUP:0x92b4 two k sz colour squares; thrust over the ship (DGROUP:0x971a +
// d = 0): the body into DGROUP:0x92b2, w8 = half(sz) - trunc8(band / 2) and the cuts for ability
// 3 by ship bytes +0xac / +0xad, the copy into DGROUP:0x92b4, then the flame at fx = half(sz) - 4,
// fy into DGROUP:0x92b2 only; thrust behind: the flame first over a black square, then the body
// through its mask, the copy into DGROUP:0x92b4 and the cuts on both. DGROUP:0x92b0 keeps both
// wings. seg15:0x28b2 for a dead ship is unread [OPEN] and logged.
#ifdef _WIN32
LaunchShip ship_20ca(Gfx& g, const VisitShipArt& v, const LaunchTodo& todo);

// seg15:0x4c76's strip rebuild on a first upward call (states 0 and 6) [BYTES seg15:0x4de1-0x4f67]
// (verified claims 56-57): PatBlt(strip, 0, 0, pw, s4 + 0x3e, BLACKNESS), so rows s4 + 0x3e ...
// s4 + 0x42 keep what they hold; BitBlt(strip, 0, s4 + 0x3e - pc - 0x34, pw, pc + 0x34, pad, 0, 0,
// SRCCOPY); a fresh mask of the body image DGROUP:0x92b0 (seg10:0x6ec6); SetStretchBltMode(3); the
// shadow (shadow_blit) to (half(pw) - half(s4), 9, s4, s4) from (0, 0, bmH, bmH) of the mask; then
// seg10:0x9962 of the mask (SRCAND) and of the body (SRCPAINT) from (0, 0, bmH, bmH) to (half(pw) -
// half(s4), 5, s4, s4): the body's top-left bmH square without the flame. A null pad or body skips
// its part [DESIGN].
void strip_rebuild(Gfx& g, Surface& strip, int pw, int pc, int s4, const Surface* pad, const Surface* body);

// seg15:0x2a08's branch B past its early return, in state 6 with a wing repair bought (DGROUP:0x953f
// != 0, DGROUP:0x9563 = 0) [BYTES seg15:0x2bff-0x2c88 seg15:0x2ca3-0x2cbe seg15:0x2cbe-0x2d81
// seg15:0x2d8f-0x2e9b] (review H2): s = trunc(bmHeight(body)/k); the square DGROUP:0x92b2 is
// overwritten in place by BitBlt(Q, 0, 0, k s, k s, body, 0, 0, SRCCOPY), so the uncut body
// DGROUP:0x92b0 (seg15:0x20ca keeps both wings in it) replaces the landing's cut square; with ship
// byte +0xe4 != 0 the flame pair at fx = half(s - 9), fy = sext BOD +0x19 of the roster's SHP, sy
// = 0x12 only when k = 4 and design word +0x44 != 0, from seg15:0x20ca's flame DGROUP:0x3cc8 (row
// design word +0x44 != 0), with design word +0x48 (behind) the PatBlt before and the body through
// its mask after. The cuts at seg15:0x2edf do not run: phase 25 has set both roster wing bytes to 1
// [BYTES seg16:0x154a-0x15e0]; a wing still 0 would read the unwritten w8, which the port logs and
// skips (ADR 0016 decision 1). Returns s.
int ship_2a08_b(Gfx& g, const Surface& body, Surface& square, const VisitShipArt& v, const LaunchTodo& todo);
#endif

// (DGROUP:0x92c6, DGROUP:0x92c8), frame 0 of seg15:0x1a42 by `Detail` [BYTES seg15:0x2855-0x28a7]
// (verified claim 37): 0 -> (4sz, trunc(16sz / 10)), 1 -> (5sz, 2sz), 2 -> (6sz, trunc(24sz / 10)),
// 3 -> (7sz, trunc(28sz / 10)); other values leave (0, 0) [DESIGN: the original keeps the words].
void jet_frame0(int sz, int detail, int& w, int& h);

// seg15:0x1a42's 16 jet frames in state 6 [BYTES seg15:0x1a4f-0x1ca1] (s6-pad.md 2.6 and the
// listing): frame 0 = jet_frame0 at y 0; with (c, dc, q) = (350, 16, 4), (450, 27, 5), (550, 39, 6),
// (650, 50, 7) for `Detail` 0 ... 3, frames 1 ... 9 w = trunc(lo16(trunc(w0 / q) trunc(c / 10)) /
// 10), h = trunc(lo16(40 w) / 100), c -= dc; then frames 15, 14, ... 10 in that order w =
// trunc(lo16(trunc(w0 / q) m) / 10), h = trunc(lo16(w p) / 100), (m, p) from (10, 100) by (+2, -12);
// each frame's y (DGROUP:0x3ce8 + 2i) the sum of the heights stored before it, every frame at x 0.
// Another `Detail` leaves (c, dc, q) and frame 0 unset in the original [OPEN]; the port returns an
// empty table then (w0 = 0) [DESIGN].
struct JetFrame {
    int w = 0, h = 0, y = 0;  // DGROUP:0x3d08 + 4i, DGROUP:0x3d0a + 4i, DGROUP:0x3ce8 + 2i
};
std::array<JetFrame, 16> jet_table(int sz, int detail);
// The height of DGROUP:0x3ccc: the sum of the 16 heights [BYTES seg15:0x1d1b-0x1d2c].
int jet_table_height(const std::array<JetFrame, 16>& t);

}  // namespace is::gfx
