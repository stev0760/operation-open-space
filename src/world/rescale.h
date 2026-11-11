// The Size change (docs/re/shell.md "Rescale (seg10:0x9a2a)", "The image rescale
// (seg10:0x6308)", "Size entry points"): the image rescale seg10:0x6308 with its copy primitive
// seg10:0x9962, the live rescale seg10:0x9a2a, the INI reread seg10:0x7f88 and the session word
// DGROUP:0x931e (`Quick`). Sources: work/analysis/m4/s8-size.md sections 1-3 as corrected by
// s8-size-verify.md (claims 1-47, Omissions O1-O12). M4 stage 8, track SIZE-CORE.
//
// The image-slot tables DGROUP:0x39e8 (DC), DGROUP:0x3ad0 (bitmap), DGROUP:0x3974 (saved object)
// and DGROUP:0x3a5c (saved palette) collapse into the port's DC word, which stands for the DC with
// its bitmap selected [DESIGN ADR 0013]. 16-bit arithmetic as the original's (ADR 0013 decision
// 7). Portable, no Win32.
#pragma once

#include <cstdint>
#include <string>

#include "world/records.h"
#include "world/sprite.h"
#include "world/world.h"

namespace is::world {

// ---- arithmetic

// `shl ax, cl; sar ax, 1` on a word, the count masked to 5 bits as a 386 does: (v << Size) sar 1,
// so Size 2 doubles, Size 1 keeps and Size 0 halves toward minus infinity [BYTES seg10:0x640b-0x641e
// seg10:0x9c07-0x9c09 seg10:0x9d35-0x9d37].
std::int16_t size_scaled(std::int16_t v, int cl);
// seg10:0x6308's W and H: size_scaled of GetObject's bmWidth / bmHeight at DGROUP:0x9156
// [BYTES seg10:0x63c5-0x641f] (verified claim 31). 869 x 53 gives 1738 x 106 at Size 2 and 434 x 26
// at Size 0.
int rescaled_extent(int extent, int size_level);

// ---- seg10:0x552(slot, 1): the forced free [BYTES seg10:0x552-0x616]

// With force 1 every slot is freed: SelectObject(DC, saved object), DeleteDC, DeleteObject(bitmap)
// and both words 0. An empty slot (DC 0) and an index outside the 58 slots do nothing.
void free_image_slot(Blitter& out, ImageSlotTable& images, int slot);

// ---- seg10:0x6308(slot, unused, hbm, mode), the image rescale (P2)

constexpr int kRescaleSlotNone = -1;   // the bitmap form: `bitmap` is rescaled and returned
constexpr int kRescaleModeForce = 2;   // mode 2 forces the rescale at Size 1 [BYTES seg10:0x6334]
constexpr int kWhiteOnBlack = 2;       // SetStretchBltMode WHITEONBLACK (slots 6 and 8)
constexpr int kSlotIconBackup = 0x20;  // DGROUP:0x3a28, never rescaled [BYTES seg10:0x9af7]
constexpr int kSlotFlameCells = 0x22;  // DGROUP:0x3a2c, seg10:0x5c42's cells [BYTES seg10:0x9b90]
// seg10:0x6308 [BYTES seg10:0x6316-0x664b] (verified claims 28-34, O8, O10):
//  - a slot >= 0 whose DC word is 0 returns 0 (a slot outside the table too, logged once);
//  - at `size_level` 1 with `mode` != 2 it returns `bitmap` unchanged;
//  - at `size_level` 2 slots 8 and 9 return 0 untouched; slot 7 (6) first frees slot 9 (8), which
//    then takes slot 7's (6's) old image, and sets `dg_740` (DGROUP:0x740) = 1;
//  - otherwise a new black image of size_scaled(w) x size_scaled(h) (Blitter::image_size of the
//    slot's image, or of `bitmap` for slot -1), stretch mode 2 for slots 6 and 8 and 3 for every
//    other (slot -1 included), the copy seg10:0x9962 (StretchBlt (0, 0, W, H) from (0, 0, w, h)
//    SRCCOPY), and the old image is deleted, except for slots 6 and 7 at Size 2, where slots 8 and 9
//    keep it;
//  - a slot returns 0 with the new image in its word; slot -1 returns the new image.
// The copy is always StretchBlt: seg10:0x9962 takes ISPACE.DLL ordinal 36 only for a source of
// 2-8 bits per pixel, and the port's images are the 8-bit index format of a display deeper than
// 8 bits (DGROUP:0x90f0 = 1, ADR 0007) [DESIGN ADR 0013] [BYTES seg10:0x9974-0x9a1d]. Which pixel
// a 2:1 shrink keeps is Wine's GDI's (ADR 0017's approach): when the Blitter hands out the source's
// pixels (Blitter::read_index) the copy is stretch_index's on every host, the new image made from
// its result; otherwise the calls go to the Blitter's stretch_blt (the recording fakes) [DESIGN ADR
// 0017 decision 2] (M4 s8 fixer, review M1).
DcId rescale_image(Blitter& out, ImageSlotTable& images, int slot, DcId bitmap, int mode,
                   int size_level, std::int16_t& dg_740);

// StretchBlt(dst 0, 0, W, H, src 0, 0, w, h, SRCCOPY) of a w x h index image into a black W x H one
// as Wine's GDI makes it (ruling R4, ADR 0017): per axis gfx::gdi_stretch_map's pixel for a
// stretch and, in COLORONCOLOR (`stretch_mode` 3), for a shrink; in WHITEONBLACK (2) a shrink ORs
// every source pixel of the walk's run (gfx::gdi_shrink_owner), so a halved mask keeps each set
// pixel. Pinned against GDI under Wine by tests/test_world_rescale.cpp (M4 s8 fixer, review M1).
std::vector<std::uint8_t> stretch_index(const std::vector<std::uint8_t>& src, int w, int h, int W, int H,
                                        int stretch_mode);

// ---- the session words of the Size change (P6)

struct SizeSession {
    // DGROUP:0x931e: INI `Quick`, stored by seg6:0xb3e at seg6:0xc65 (start-up seg6:0xae7, the
    // Options dialog's seg10:0x7f52); read by the sheet `sel` tests seg14:0x1274-0x1290, seg10:0x19ac-
    // 0x19c1 and seg10:0x53c2-0x53d7; 0 during seg10:0x9a2a, whose beep path leaves it 0 (a fault of
    // the original the port keeps, shell.md) [BYTES seg6:0xc65 seg10:0x9a50-0x9a73 seg10:0xa108]
    // (verified claims 2, 3).
    std::int16_t quick = 0;
    // DGROUP:0x92e2: 1 makes the next load reread INI `Size` (load_size_reread, seg9:0x23e4);
    // seg10:0x7f88 sets it when no ship sheet exists [BYTES seg10:0x803c] and the Restore's header
    // read when the saved Size differs (dialogs/save_game.cpp step 6) [BYTES seg6:0x95ed]. Its one
    // home (M4 s8 fixer, review L4).
    std::int16_t dg_92e2 = 0;
    // DGROUP:0x103a = 1 on the reload path; its only reader clears it (pitfalls.md) [BYTES seg10:0x9ab6].
    std::int16_t dg_103a = 0;
    // DGROUP:0x934c (the "Resizing graphics" dialog, non-zero while it exists) and DGROUP:0x9518.
    std::int16_t dialog = 0;
    std::int16_t dg_9518 = 0;
    // The INI value outside 0 ... 2 that the last refused read (size_from_ini) stood Size 1 in for:
    // the original stored it in DGROUP:0x9156, so seg10:0x7f88 finds no difference while the INI
    // still holds it. A read of a value in range and a completed seg10:0x9a2a step end it (ruling
    // R2, review M3) [DESIGN ADR 0016 decision 2].
    bool refused = false;
    std::int16_t refused_ini = 0;
};
SizeSession& size_session();
// seg6:0xb3e's store of INI `Quick` (GetPrivateProfileInt("Quick", 0) at seg6:0xc60) into
// DGROUP:0x931e [BYTES seg6:0xc60 seg6:0xc65]. The program calls it where seg6:0xb3e runs.
void load_quick(int ini_quick);
// DGROUP:0x931e for the three `sel` tests.
std::int16_t quick_word();

// An INI `Size` stored into the port's DGROUP:0x9156: the original stores any value (an INI `Size`
// of 3 runs with DGROUP:0x9150 = 1 and the ship in two halves, core-0011), but outside 0 ... 2 the
// port's sheet build indexes its pixel tables outside their storage, so such a value is refused
// and Size 1 stands in for it, logged once (ruling R2) [DESIGN ADR 0016 decision 2]. The two reads
// that store (the start-up seg6:0xaf8 and the load's reread seg9:0x23f9) go through it and record
// the refused value in SizeSession::refused_ini, which seg10:0x7f88's compare takes as the stored
// word (size_reread) (M4 s8 fixer, review M3). `address` names the read for the log.
std::int16_t size_from_ini(int ini, const char* address);

// ---- the host of seg10:0x9a2a and seg10:0x7f88 (P4, P5)

// What the rescale reaches outside the World: the drawing side's image table and Blitter, the
// window calls and the builders. Every default does nothing; the M5 and unported parts log once
// through todo() (ruling R3).
class RescaleHost {
public:
    virtual ~RescaleHost() = default;

    // The image table DGROUP:0x39e8 and its Blitter; null: no images are touched (the size words
    // and the INI still change).
    virtual Blitter* blitter() { return nullptr; }
    virtual ImageSlotTable* images() { return nullptr; }

    // DGROUP words the World does not hold.
    virtual std::int16_t screen_state() { return 1; }  // DGROUP:0x90f4
    virtual std::int16_t enhance() { return 1; }       // DGROUP:0x8fe8 INI `Enhance` (default 1)
    // DGROUP:0x90f0: 1 at 8 bits per pixel and more, which the port's surface is (ADR 0007).
    virtual std::int16_t deep() { return 1; }
    // DGROUP:0x103c = 1: the wave frame (seg7:0xb73) calls seg10:0x7f88 next (update.h
    // FrameGlobals::size_reread).
    virtual void set_size_reread() {}

    // The window calls.
    virtual void beep() {}  // MessageBeep(0) [BYTES seg10:0x9a6c-0x9a6e]
    // CreateDialog(ENG_LANG.DLL, DIALOG 101, main, seg9:0x90fe), modeless; WM_INITDIALOG with
    // DGROUP:0x9518 = 1 puts string 2269 into control 114 and centres it on the screen. True when
    // the window exists (DGROUP:0x934c != 0) [BYTES seg10:0x9a76-0x9a9a].
    virtual bool open_dialog() { return false; }
    // DGROUP:0x8f64 = 0 and DestroyWindow(DGROUP:0x934c) [BYTES seg10:0xa0c6-0xa0d0].
    virtual void close_dialog() {}
    // PostMessage(DGROUP:0x16, msg, 0, 0): 0x978 after the dialog closes [BYTES seg10:0xa0db-0xa0ea].
    virtual void post_main(std::uint32_t msg) { (void)msg; }
    // WritePrivateProfileString(DGROUP:0x9690 "Inner Space", key, seg6:0xf1a(value), DGROUP:0x96a9)
    // [BYTES seg10:0xa07b-0xa0ba].
    virtual void write_ini(const char* key, int value) { (void)key, (void)value; }
    // GetPrivateProfileInt(DGROUP:0x9690, "Size", 1, DGROUP:0x96a9) [BYTES seg10:0x7fa0].
    virtual int ini_size() { return 1; }
    // The port's copies of image-slot words (the HUD's DGROUP:0x3a38, the strip DC, the weapon
    // sheet) are the same locations as the table's (ADR 0016 decision 3): called once at the end
    // of a pass that changed the table [DESIGN].
    virtual void images_changed() {}
    // After seg10:0x522(1) emptied the whole table (load_size_reread): the port's own holders of
    // slot words (the strip DC of slot 13, the gate DC, the sheet copies) let go of them, so the
    // builders that follow make the images again [DESIGN ADR 0016 decision 3].
    virtual void slots_freed() {}
    // The log once of an unported or M5 part (ruling R3).
    virtual void todo(const std::string& what, const char* address) { (void)what, (void)address; }

    // The fragments' bitmaps DGROUP:0x4e94 + 2i (32 words in the original, wave_end.h); null for
    // an index the port does not hold [BYTES seg10:0x9b4d-0x9b80].
    virtual DcId* fragment_word(int i) { (void)i; return nullptr; }
    // seg10:0x5c42: the flame cells DGROUP:0x3c60 and slot 0x22 at the new Size [BYTES
    // seg10:0x9b95] (player_sheet.h flame_strip_create).
    virtual void flame_cells() {}

    // ---- the ship block [BYTES seg10:0x9cd7-0x9f5c]
    // seg14:0x11a8(i): ship i's sheet into the freed slot 0x28 + i at the new Size, with
    // quick_word() = 0 [BYTES seg10:0x9e10].
    virtual void ship_sheet(int i) { (void)i; }
    // seg8:0x2cd8(i, wing) with DGROUP:0x92e4 (World::restored) = 1: no taunts, no debris, no
    // sound; ship dword +0x156 = now - 10000, the wing byte cleared, design +0x3c recomputed and
    // ordinal 15's cut with the new size words [BYTES seg10:0x9d8a seg10:0x9d9d] (O5).
    virtual void wing_cut(int i, int wing) { (void)i, (void)wing; }
    // seg10:0x18b0(a, b, c, mirror, cls), the hazard image (M5): returns the word stored at
    // DGROUP:0x3d6e + 0x34j. The default logs once and returns `keep` (the word unchanged).
    virtual std::int16_t hazard_image(std::int16_t a, std::int16_t b, std::int16_t c, int mirror,
                                      int cls, std::int16_t keep);
    // seg10:0x533a(w): the launched weapon's image slot (rock_images.h weapon_image_slot, then
    // the sheet) [BYTES seg10:0x9f28]. Default: logged once.
    virtual void weapon_image(int w);

    // ---- seg10:0x9568's builders, each after its slots' free (the reload path) [BYTES
    // seg10:0x9572-0x9668]. Defaults log once.
    virtual void gate_images();    // seg10:0x3ea2, slots 1, 2
    virtual void logo_cells();     // seg10:0x58ce(1), slot 0x16
    virtual void tank_image();     // seg10:0x565a, slot 4
    virtual void smoke_image();    // seg10:0x5202, slot 0xc
    // seg10:0x39a8(1), slot 5: when the slot is empty, the strip made and then seg10:0x6308(5, -1,
    // -1, -1) at DGROUP:0x9156; a slot that holds an image is left alone, rescale included [BYTES
    // seg10:0x39b2-0x39b7 seg10:0x3a15-0x3a24].
    virtual void logo_strip();
    virtual void rock_images();    // seg10:0x3a2e, slots 6 ... 9 (clears DGROUP:0x740)
    virtual void effect_images();  // seg10:0x5102, slots 0xa, 0xb
    virtual void pod_image();      // seg10:0x42ee, slot 0x1d
    virtual void hook_sheet();     // seg10:0x561a, slot 3
    virtual void def_image();      // seg10:0x464a, slot 0 (BITMAP "DEF", pool 2; M5)
    virtual void race_image();     // seg10:0x49d4, slot 0x1e (a race, DGROUP:0x9320 = 6; M5)
    // The rest of the reload path [BYTES seg10:0x9f68-0x9fd7].
    virtual void icon_strip();     // seg10:0x5b30, slot 0xd unscaled (after its free)
    virtual void icon_overlays();  // seg9:0x94ba (8 rand() in a kind-2 wave with DGROUP:0x9334 != 0)
    virtual void icon_crack(int i) { (void)i; }  // seg7:0x233c(0, i, 0): no sound, no rand()

    // ---- the design-word tail, every ship [BYTES seg10:0x9fd9-0xa078]
    // Design word +0x04 (DGROUP:0x96d6 + 0xad·n); null when the port does not hold it.
    virtual std::int16_t* design_word_04(int n) { (void)n; return nullptr; }
    virtual void barrel_table(int n) { (void)n; }  // seg10:0x1502(n)
    virtual void flame_strip(int i) { (void)i; }   // seg10:0x5d3c(i)
};

// The design words seg10:0x9a2a's tail writes from design +0x2a `side2a` and +0x04 `w04` at the
// new Size [BYTES seg10:0x9fe0-0xa035] (verified claims 24, 26): side = side2a, plus 1 at Size 0
// when odd; +0x00 = +0x02 = (2·(side sar Size)) / 2 (= side sar Size); +0x04 = 4 - w04 - side / 2
// (toward zero) when w04 < 0, else w04. The TRANSFORMER's 33 gives 8 at Size 2 and 34 at Size 0.
struct RescaledDesign {
    std::int16_t w00 = 0, w02 = 0, w04 = 0;
};
RescaledDesign rescaled_design(std::int16_t side2a, std::int16_t w04, int new_size);

// The size words of one object record in the pool loop [BYTES seg10:0x9bbc-0x9ca5] (verified
// claims 12-14): at new Size 0 a ship-pool record with an odd +0x22 first gets +0x22 + 1 and
// +0x24 - 1; then +0x22 and +0x24 = lo16(v << (delta + 1)) sar 1; a pool-3 record whose new +0x22
// is below 12 << new Size gets +0x24 = 4 when delta = 1 and new Size > 0, and also when new
// Size = 1 with `enhance` != 0 and `deep` = 0; last +0x12 = +0x24. `extent` +0x20 is untouched.
void rescale_object_words(ObjectRecord& o, int pool, int ship_pool, int delta, int new_size,
                          std::int16_t enhance, std::int16_t deep);

// seg10:0x9a2a(delta) (P4) [BYTES seg10:0x9a2a-0xa10e]: outside state 1 or in seg18 mode only
// DGROUP:0x103c = 1 (a press there has no later effect, O1); delta + Size outside 0 ... 2 beeps and
// leaves `Quick` 0; else the dialog, the images at the temporary Size delta + 1, the new Size,
// the size words of pools DGROUP:0x9326 ... DGROUP:0x9324, the ship block (reload, or new Size 2
// with `Enhance`), the reload path (new Size 1 by Ctrl +, or by Ctrl - below 8 bpp with
// `Enhance`), the design words and flame strips of every ship, INI `Size` (and `SetSize` in a
// player game), the dialog's end, BLACKNESS on the play area and `Quick` restored. `delta` is the
// callers' +-1.
void rescale_step(World& w, int delta, RescaleHost& h);

// seg10:0x7f88 (P5) [BYTES seg10:0x7f95-0x8042] (verified claim 36, O9): INI `Size` against
// DGROUP:0x9156; on a difference with ship sheet slot 0x28 (DGROUP:0x3a38) empty, DGROUP:0x92e2 = 1
// and DGROUP:0x103c = 1; else one rescale_step of the difference's sign for a difference of +-1,
// two for any other (an INI value outside 0 ... 2 beeps on the second, ruling R2). An INI value
// equal to the one a refused read recorded (SizeSession::refused_ini) is no difference, as the
// original, which stored it, finds none [DESIGN ADR 0016 decision 2] (review M3). The GetDC /
// ReleaseDC of DGROUP:0x13dc around the steps stand for the window DC the port keeps [DESIGN ADR
// 0013]; the palette select with its stale saved word (O9) selects nothing [DESIGN ADR 0016
// decision 1].
void size_reread(World& w, RescaleHost& h);

// seg9:0x23e4-0x2418, in the LOADING branch of the load seg9:0x22b4(0) that seg10:0x0 runs at a
// game's start (the briefing entry's seg9:0x22b4(1) skips it, DGROUP:0x92e6 = 1): with
// DGROUP:0x92e2 != 0, INI `Size` (size_from_ini) into DGROUP:0x9156, DGROUP:0x9150 = 4 - Size,
// seg10:0x522(1) (seg10:0x552(k, 1) for k = 0 ... 0x39) and seg10:0x5c42; then DGROUP:0x92e2 = 0
// whatever it held [BYTES seg9:0x22e9 seg9:0x23e4-0x2418 seg10:0x522-0x548] (verified claim 39).
// No other load reads INI `Size`: without the flag the wave loads at the Size in force and a
// pending DGROUP:0x103c rescales it in the first frame (M4 s8 fixer, review H1).
void load_size_reread(World& w, RescaleHost& h);

// seg9:0xc06's tail before seg9:0x5cca, at a new or restored game's state change to state 1: when
// DGROUP:0x9156 != 1, seg10:0x552(5, 1), seg10:0x39a8(1) (the logo strip made at the Size) and
// seg10:0x552(0x16, 1) [BYTES seg9:0xd38-0xd5c] (M4 s8 fixer, review H2).
void new_game_logo(World& w, RescaleHost& h);

}  // namespace is::world
