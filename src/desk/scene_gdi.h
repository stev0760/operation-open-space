// The GDI side of ISPACE.DLL ordinal 32 (seg8:0x0): the op list of src/desk/scene.h replayed on a
// DC, and the device texts of ISPACE.DLL seg8:0x5c86 read from the system as the original reads
// them (work/analysis/m4/s6-scene.md 1.9, 1.10, 5.2, 5.3; ruling R3). The desk scene
// (src/desk/gdi_desk.cpp) and the landing scene (src/hud/visit_win32.cpp) draw through it.
// Win32 only; the host preset builds an empty translation unit.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>

#include "desk/scene.h"

namespace is::gfx {
class Gfx;
}
namespace is::assets {
class AssetStore;
}

namespace is::desk {

// What the replay needs besides the op list.
struct SceneGdi {
    // The COLORREF the DC takes for a colour the bytes build (the PALETTERGB values of
    // seg8:0x27f0 and seg8:0x5978, the plain RGB greys of ordinal 34); the value itself when
    // empty (a true-colour DC, the tests).
    std::function<COLORREF(std::uint32_t)> colour;
    // The bitmap of a pattern brush (METAL, graphic 76), kept by the caller until draw_scene
    // returns; null leaves the DC's own brush selected for that PatBlt, as a failed
    // CreatePatternBrush leaves the original's (Pattern::kNone, ADR 0016 decision 1) [DESIGN].
    std::function<HBITMAP(Pattern)> pattern;
};

// Replays r.ops on dc in order: SetStretchBltMode(dc, COLORONCOLOR) first [BYTES ISPACE.DLL
// seg8:0x302], then each brush, pen, PatBlt (ROP and signed extents as the op has them), line,
// text and ordinal-34 frame. The DC's brush, pen, font, colours, alignment and background mode are
// as before on return. Text goes through ExtTextOutW with the advances of GetTextExtentExPointW
// (ADR 0007 Consequences) [DESIGN].
void draw_scene(HDC dc, const Render& r, const SceneGdi& with = {});

// The port's form on the renderer of src/gfx: every colour the ops name is allocated through
// palette command 6 in drawing order and published (command 3), so the 8-bit surface shows the
// nearest entries (ADR 0007, "Solid colours ... land on the nearest palette entry") [DESIGN];
// METAL comes from ordinal 3 (Gfx::load_bitmap) [BYTES ISPACE.DLL seg8:0x34cd-0x34d1] and graphic
// 76 from ordinal 1 (Gfx::load_graphic(76, 0, -1)) [BYTES ISPACE.DLL seg8:0x34de-0x34ef], each
// loaded once per call and only when an op names it. `todo` hears a pattern that did not load.
void draw_scene(gfx::Gfx& g, HDC dc, const Render& r,
                const std::function<void(const std::string&, const char*)>& todo = {});

// The program's ordinal 32 through seg10:0x9526 (cdecl; hdc DGROUP:0x13dc and hPal DGROUP:0x920e
// added) [BYTES seg10:0x9530-0x955d]: IGRAPHIC.DLL #6600 id from the store, the device texts of
// read_devices(), LoadString through `strings` (ENG_LANG.DLL, ISPACE.DLL DGROUP:0x690), the
// override words ox, oy and the Z delta, cx and cy for SM_CXSCREEN and SM_CYSCREEN. Throws what
// the store throws.
Render render_scene(const assets::AssetStore& store, int id, int W, int H, int cx, int cy,
                    std::function<std::string(int)> strings, int ox = 0, int oy = 0, int z_delta = 0);

// ISPACE.DLL seg8:0x5c86 on the system (ruling R3): GetPrivateProfileStringA on
// GetWindowsDirectoryA's system.ini, control.ini and protocol.ini and GetSystemDirectoryA's
// network.inf, waveOutGetNumDevs and waveOutGetDevCapsA(0), and kWinFlagsPort for GetWinFlags
// [BYTES ISPACE.DLL seg8:0x5c95-0x5fa5] [DESIGN].
Conditions read_devices();

}  // namespace is::desk
#endif
