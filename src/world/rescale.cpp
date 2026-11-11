// The Size change: seg10:0x6308, seg10:0x9a2a, seg10:0x7f88 and DGROUP:0x931e (rescale.h).
// Sources: work/analysis/m4/s8-size.md sections 1-3 with s8-size-verify.md (the verify file wins
// where they differ). M4 stage 8, track SIZE-CORE.
#include "world/rescale.h"

#include <array>
#include <vector>

#include "gfx/ship_quick.h"
#include "world/design_words.h"
#include "world/missile.h"

namespace is::world {

namespace {

// The 386's count: `shl ax, cl` and `sar ax, cl` use cl & 0x1f; cl is the low byte of the word
// moved into it (`mov cl, byte ptr [...]`).
int count_of(int v) { return (v & 0xff) & 0x1f; }

// The log once of a host default or an M5 part (ruling R3).
void log_once(RescaleHost& h, bool& once, const char* what, const char* address) {
    if (once) return;
    once = true;
    h.todo(what, address);
}

bool g_once_slot_range = false;
bool g_once_unmeasured = false;
bool g_once_create = false;
bool g_once_fragment = false;
bool g_once_pool = false;
bool g_once_record = false;
bool g_once_ship = false;
bool g_once_roster = false;
bool g_once_design = false;
bool g_once_design04 = false;
bool g_once_class = false;
bool g_once_pool8 = false;
bool g_once_cache = false;
bool g_once_kind = false;
bool g_once_hazard = false;
bool g_once_weapon = false;
bool g_once_builder[13] = {};
bool g_once_ini_size = false;

// One axis of stretch_index: the source indices each destination index takes, a contiguous run
// [first, last] (a single index unless a WHITEONBLACK shrink ORs a run).
struct AxisRun {
    std::vector<int> first, last;
};
AxisRun axis_runs(int src_len, int dst_len, bool rows, bool orscans) {
    AxisRun a;
    a.first.assign(static_cast<std::size_t>(dst_len), 0);
    a.last.assign(static_cast<std::size_t>(dst_len), -1);
    if (!orscans || dst_len >= src_len) {
        const std::vector<int> m = gfx::gdi_stretch_map(src_len, dst_len, rows);
        for (int d = 0; d < dst_len; ++d) a.first[static_cast<std::size_t>(d)] = a.last[static_cast<std::size_t>(d)] = m[static_cast<std::size_t>(d)];
        return a;
    }
    // A halving to one pixel (2 or 3 to 1) ORs the run [0, (src - 1) / 2] on either axis under
    // Wine's GDI, where the walk below would take every source index; the COLORONCOLOR rule of
    // gdi_stretch_map's one-pixel column, the middle, is this run's last [DESIGN: measured with
    // the Win32 test, sources 2 and 3] (M4 s8 fixer, review M1).
    if (dst_len == 1) {
        a.first[0] = 0;
        a.last[0] = (src_len - 1) / 2;
        return a;
    }
    const std::vector<int> owner = gfx::gdi_shrink_owner(src_len, dst_len);
    for (int s = 0; s < src_len; ++s) {
        const int d = owner[static_cast<std::size_t>(s)];
        if (d < 0 || d >= dst_len) continue;
        if (a.last[static_cast<std::size_t>(d)] < 0) a.first[static_cast<std::size_t>(d)] = s;
        a.last[static_cast<std::size_t>(d)] = s;
    }
    return a;
}

SizeSession g_session;

DcId& slot_word(ImageSlotTable& images, int k) { return images[static_cast<std::size_t>(k)]; }

}  // namespace

std::int16_t size_scaled(std::int16_t v, int cl) { return sar16(shl16(v, count_of(cl)), 1); }

int rescaled_extent(int extent, int size_level) { return size_scaled(lo16(extent), size_level); }

std::vector<std::uint8_t> stretch_index(const std::vector<std::uint8_t>& src, int w, int h, int W, int H,
                                        int stretch_mode) {
    std::vector<std::uint8_t> dst(W > 0 && H > 0 ? static_cast<std::size_t>(W) * static_cast<std::size_t>(H) : 0, 0);
    if (w <= 0 || h <= 0 || W <= 0 || H <= 0 || src.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(h))
        return dst;
    const bool orscans = stretch_mode == kWhiteOnBlack;
    const AxisRun ax = axis_runs(w, W, false, orscans), ay = axis_runs(h, H, true, orscans);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            std::uint8_t v = 0;  // the black the new image starts as (PatBlt BLACKNESS)
            for (int sy = ay.first[static_cast<std::size_t>(y)]; sy <= ay.last[static_cast<std::size_t>(y)]; ++sy)
                for (int sx = ax.first[static_cast<std::size_t>(x)]; sx <= ax.last[static_cast<std::size_t>(x)]; ++sx)
                    v = static_cast<std::uint8_t>(v | src[static_cast<std::size_t>(sy) * static_cast<std::size_t>(w) +
                                                           static_cast<std::size_t>(sx)]);
            dst[static_cast<std::size_t>(y) * static_cast<std::size_t>(W) + static_cast<std::size_t>(x)] = v;
        }
    return dst;
}

// seg10:0x552(slot, force) with force 1: the keep list of force 0 (slots 1, 2, 4 ... 0xc, 0x16,
// 0x22) is skipped, so the slot's DC goes back to its saved object, its palette (when PAL != 0) is
// selected back, the DC and the bitmap are deleted and both words cleared [BYTES seg10:0x562-0x569
// seg10:0x56c-0x570 seg10:0x5bd-0x60b]. The port's DC word stands for all four [DESIGN ADR 0013].
void free_image_slot(Blitter& out, ImageSlotTable& images, int slot) {
    if (slot < 0 || slot >= kImageSlots) {
        // DGROUP:0x39e8 + 2·slot lies outside the table: a read of 0 frees nothing [DESIGN ADR
        // 0016 decision 2].
        guard_todo(g_once_slot_range, "an image slot outside the 58 of DGROUP:0x39e8", "seg10:0x552");
        return;
    }
    DcId& d = slot_word(images, slot);
    if (d == kDcNone) return;  // [BYTES seg10:0x562-0x569]
    out.delete_image(d);
    d = kDcNone;  // [BYTES seg10:0x5f5 seg10:0x60b]
}

DcId rescale_image(Blitter& out, ImageSlotTable& images, int slot, DcId bitmap, int mode,
                   int size_level, std::int16_t& dg_740) {
    // 1. An empty slot returns 0; the test reads DGROUP:0x39e6 for slot -1 and ignores it
    // [BYTES seg10:0x6316-0x632a] (verified claim 28).
    if (slot != kRescaleSlotNone) {
        if (slot < 0 || slot >= kImageSlots) {
            guard_todo(g_once_slot_range, "an image slot outside the 58 of DGROUP:0x39e8", "seg10:0x6316");
            return kDcNone;  // [DESIGN ADR 0016 decision 2]
        }
        if (slot_word(images, slot) == kDcNone) return kDcNone;
    }
    // 2. Size 1 without mode 2 returns the hbm argument, before the slot's bitmap replaces it
    // [BYTES seg10:0x632d-0x633d seg10:0x6334]; `mode` is compared with 2 only (O8).
    if (size_level == 1 && mode != kRescaleModeForce) return bitmap;
    const DcId src = slot == kRescaleSlotNone ? bitmap : slot_word(images, slot);  // [BYTES seg10:0x6340-0x634f]
    // 3. At Size 2 slots 8 and 9 are left alone [BYTES seg10:0x6352-0x6363] (claim 29).
    if (size_level == 2 && (slot == 9 || slot == 8)) return kDcNone;
    // 4. GetObject(hbm, 14, &bm) [BYTES seg10:0x63c8] (claim 31). A bitmap the Blitter does not hold is
    // a failed GetObject, which leaves the BITMAP on the stack unwritten; the port makes nothing
    // and hands the input back [DESIGN ADR 0016 decision 1].
    int w = 0, h = 0;
    if (!out.image_size(src, w, h)) {
        guard_todo(g_once_unmeasured, "an image the Blitter cannot measure", "seg10:0x63c8");
        return slot == kRescaleSlotNone ? bitmap : kDcNone;
    }
    // 5. Slots 6 and 7 at Size 2: slot 8 (9) is freed and takes slot 6's (7's) DC, bitmap, saved
    // object and palette words, and DGROUP:0x740 = 1 [BYTES seg10:0x6365-0x63bf seg10:0x638f
    // seg10:0x63bf] (claim 30). The old image lives on there: no DeleteDC (seg10:0x647b-0x648d)
    // and no DeleteObject (seg10:0x65ce-0x65f9).
    const bool kept = size_level == 2 && (slot == 7 || slot == 6);
    if (kept) {
        const int to = slot + 2;
        free_image_slot(out, images, to);
        slot_word(images, to) = slot_word(images, slot);
        dg_740 = 1;
    }
    // 6. The new bitmap, W = lo16(w << Size) sar 1 by H likewise, recomputed at each use [BYTES
    // seg10:0x640b-0x641e seg10:0x64ab-0x64be seg10:0x651a-0x652d seg10:0x6566-0x6579]. For a slot
    // it is CreateCompatibleBitmap of the old slot DC with the old bitmap still selected, so it
    // takes that bitmap's format (O8); the port's images are all of the renderer's index format,
    // the monochrome slots 6 and 8 included (rock_images.h) [DESIGN ADR 0013]. A zero extent makes
    // a 1 x 1 bitmap [REF Wine 11.0 dlls/gdi32 CreateCompatibleBitmap].
    const int W = size_scaled(lo16(w), size_level), H = size_scaled(lo16(h), size_level);
    const int mw = W > 0 && H > 0 ? W : 1, mh = W > 0 && H > 0 ? H : 1;
    // [BYTES seg10:0x6537-0x654f] (claim 32): WHITEONBLACK ORs the pixels a shrink drops.
    const int stretch_mode = (slot == 8 || slot == 6) ? kWhiteOnBlack : kColorOnColor;
    // With the source's pixels in hand the copy is Wine's StretchBlt rule on every host
    // (stretch_index), not the host GDI's: the image the PatBlt, SetStretchBltMode and StretchBlt
    // below would make [DESIGN ADR 0017 decision 2] (M4 s8 fixer, review M1).
    std::vector<std::uint8_t> px;
    int pw = 0, ph = 0;
    const bool copied = out.read_index(src, px, pw, ph) && pw == w && ph == h;
    const DcId made = copied ? out.create_image_from(mw, mh,
                                                      W > 0 && H > 0 ? stretch_index(px, w, h, W, H, stretch_mode)
                                                                     : std::vector<std::uint8_t>(1, 0))
                             : out.create_image(mw, mh);
    if (made == kDcNone) {
        // The port could not make the image (no surface): the slot keeps its picture [DESIGN].
        guard_todo(g_once_create, "the rescale could not make its image", "seg10:0x6407");
        return slot == kRescaleSlotNone ? bitmap : kDcNone;
    }
    // The palette selects of seg10:0x64c7-0x64f4 and the saved palette word are the DC word's
    // [DESIGN ADR 0013]: with DGROUP:0x920e = 0 the original stores a stale stack word into
    // DGROUP:0x3a5c + 2·slot (seg10:0x661c), the port's equivalent of which is no palette (0)
    // [DESIGN ADR 0016 decision 1] (O8).
    if (!copied) {
        out.pat_blt(made, 0, 0, W, H, kBlackness);  // [BYTES seg10:0x6513-0x6532]
        out.set_stretch_mode(made, stretch_mode);
        // seg10:0x9962(1, newDC, 0, 0, W, H, srcDC, 0, 0, w, h, SRCCOPY) [BYTES seg10:0x6554-0x6587]:
        // StretchBlt at the port's depth (rescale.h) [BYTES seg10:0x9974-0x9a1d] (claim 35, O10).
        out.stretch_blt(made, 0, 0, W, H, src, 0, 0, w, h, kSrcCopy);
    }
    // DeleteDC of the source DC and of the old slot DC, DeleteObject(hbm) [BYTES seg10:0x65c4]
    // (claim 33): one delete of the port's image; slots 6 and 7 at Size 2 keep it in 8 and 9.
    if (!kept) out.delete_image(src);
    if (slot == kRescaleSlotNone) return made;  // [BYTES seg10:0x6626-0x664b]
    slot_word(images, slot) = made;              // [BYTES seg10:0x65fc-0x6623]
    return kDcNone;
}

// ---- the session words

SizeSession& size_session() { return g_session; }
void load_quick(int ini_quick) { g_session.quick = lo16(ini_quick); }  // [BYTES seg6:0xc65]
std::int16_t quick_word() { return g_session.quick; }

std::int16_t size_from_ini(int ini, const char* address) {
    SizeSession& ss = size_session();
    const std::int16_t v = lo16(ini);
    if (v >= 0 && v <= 2) {
        ss.refused = false;
        return v;
    }
    guard_todo(g_once_ini_size,
               "an INI Size outside 0 ... 2 (the port's tables would be indexed outside their storage): Size 1",
               address);  // ruling R2 [DESIGN ADR 0016 decision 2]
    ss.refused = true;
    ss.refused_ini = v;
    return 1;
}

// ---- the host defaults (ruling R3: logged once)

std::int16_t RescaleHost::hazard_image(std::int16_t a, std::int16_t b, std::int16_t c, int mirror,
                                       int cls, std::int16_t keep) {
    (void)a, (void)b, (void)c, (void)mirror, (void)cls;
    log_once(*this, g_once_hazard, "the class >= 5 hazard images (M5)", "seg10:0x18b0");
    return keep;
}
void RescaleHost::weapon_image(int w) {
    (void)w;
    log_once(*this, g_once_weapon, "a hazard's weapon image (M5)", "seg10:0x533a");
}
void RescaleHost::gate_images() { log_once(*this, g_once_builder[0], "the gate images", "seg10:0x3ea2"); }
void RescaleHost::logo_cells() { log_once(*this, g_once_builder[1], "the logo cells", "seg10:0x58ce"); }
void RescaleHost::tank_image() { log_once(*this, g_once_builder[2], "the fuel can", "seg10:0x565a"); }
void RescaleHost::smoke_image() { log_once(*this, g_once_builder[3], "the smoke strip", "seg10:0x5202"); }
void RescaleHost::logo_strip() { log_once(*this, g_once_builder[4], "the logo strip", "seg10:0x39a8"); }
void RescaleHost::rock_images() { log_once(*this, g_once_builder[5], "the rock images", "seg10:0x3a2e"); }
void RescaleHost::effect_images() { log_once(*this, g_once_builder[6], "the fireballs", "seg10:0x5102"); }
void RescaleHost::pod_image() { log_once(*this, g_once_builder[7], "the pods", "seg10:0x42ee"); }
void RescaleHost::hook_sheet() { log_once(*this, g_once_builder[8], "the hook sheet", "seg10:0x561a"); }
void RescaleHost::def_image() { log_once(*this, g_once_builder[9], "BITMAP DEF, pool 2 (M5)", "seg10:0x464a"); }
void RescaleHost::race_image() { log_once(*this, g_once_builder[10], "the race image (M5)", "seg10:0x49d4"); }
void RescaleHost::icon_strip() { log_once(*this, g_once_builder[11], "the icon strip", "seg10:0x5b30"); }
void RescaleHost::icon_overlays() { log_once(*this, g_once_builder[12], "the icon overlays", "seg9:0x94ba"); }

// ---- arithmetic of the pass

RescaledDesign rescaled_design(std::int16_t side2a, std::int16_t w04, int new_size) {
    std::int16_t side = side2a;  // [bp - 0xa] = design +0x2a [BYTES seg10:0xa066-0xa06a]
    // At Size 0 an odd side is raised by 1; [bp - 0xc] (6, 5 or 4) is never read (O6) [BYTES
    // seg10:0x9fe5-0x9ff5 seg10:0x9fe0 seg10:0xa073].
    if (new_size == 0 && (side & 1) != 0) side = add16(side, 1);
    // sar ax, cl; add ax, ax; cwd; sub ax, dx; sar ax, 1 [BYTES seg10:0xa001-0xa008] (claim 24).
    std::int16_t s = sar16(side, count_of(new_size));
    s = add16(s, s);
    s = idiv16(s, 2);
    RescaledDesign r;
    r.w00 = r.w02 = s;  // [BYTES seg10:0xa015 seg10:0xa019]
    r.w04 = w04;
    // 4 - (+0x04) - side / 2, side / 2 by cwd; sub; sar of the raised local (claim 26) [BYTES
    // seg10:0xa01d-0xa035 seg10:0xa02c].
    if (w04 < 0) r.w04 = sub16(sub16(4, w04), idiv16(side, 2));
    return r;
}

void rescale_object_words(ObjectRecord& o, int pool, int ship_pool, int delta, int new_size,
                          std::int16_t enhance, std::int16_t deep) {
    // [BYTES seg10:0x9bbc-0x9bec] (claim 13): before the shifts, with DGROUP:0x9156 the new Size.
    if (new_size == 0 && pool == ship_pool && (o.size & 1) != 0) {
        o.size = add16(o.size, 1);
        o.gap = sub16(o.gap, 1);
    }
    // mov cl, byte [bp + 6]; inc cl: 2 for +1, 0 for -1 [BYTES seg10:0x9bf0-0x9c1f] (claim 12).
    const int cl = (delta + 1) & 0xff;
    o.size = size_scaled(o.size, cl);
    o.gap = size_scaled(o.gap, cl);
    // The small rocks of pool 3 (the literal 3), both compares signed on the scaled +0x22 against
    // 12 << new Size [BYTES seg10:0x9c23-0x9c4b seg10:0x9c51-0x9c8b] (claim 14).
    const std::int16_t limit = shl16(12, count_of(new_size));
    if (pool == 3 && delta == 1 && new_size > 0 && o.size < limit) o.gap = 4;
    if (pool == 3 && new_size == 1 && o.size < limit && enhance != 0 && deep == 0) o.gap = 4;
    o.src_y = o.gap;  // +0x12 = +0x24 [BYTES seg10:0x9c91-0x9ca5]
}

// ---- seg10:0x9a2a

void rescale_step(World& w, int delta, RescaleHost& h) {
    SizeSession& ss = size_session();
    // 1.1 The gate: only DGROUP:0x103c = 1, and the epilogue skips the Quick restore [BYTES
    // seg10:0x9a39-0x9a4d] (claim 1). The press changes neither INI `Size` nor DGROUP:0x9156, so
    // the wave frame's seg10:0x7f88 finds no difference unless one existed already (O1): the delta
    // is not queued.
    if (h.screen_state() != 1 || w.w9130 != 0) {
        h.set_size_reread();
        return;
    }
    const std::int16_t saved = ss.quick;  // [bp - 0x10] [BYTES seg10:0x9a50-0x9a56]
    ss.quick = 0;
    const std::int16_t d = lo16(delta);
    const std::int16_t new_size = add16(d, w.size);  // [bp - 8] [BYTES seg10:0x9a5c-0x9a5f seg10:0x9aa0]
    // Beyond 0 ... 2 (signed): MessageBeep(0) and the jump past the restore, so `Quick` stays 0,
    // a fault of the original the port keeps (ruling R1) [BYTES seg10:0x9a63-0x9a73] (claim 2).
    if (new_size > 2 || new_size < 0) {
        h.beep();
        return;
    }
    // The modeless dialog [BYTES seg10:0x9a76-0x9a9a] (claim 4).
    if (ss.dialog == 0) {
        ss.dg_9518 = 1;
        ss.dialog = h.open_dialog() ? 1 : 0;
    }
    // 1.2 The reload flag, the local [bp - 0x14] [BYTES seg10:0x9aa4-0x9ad5] (claim 5).
    const std::int16_t enhance = h.enhance(), deep = h.deep();
    const bool reload = (new_size == 1 && d == 1) ||
                        (new_size == 1 && d == -1 && enhance != 0 && deep == 0);
    if (reload) ss.dg_103a = 1;  // no branch reads it (claim 6) [BYTES seg10:0x9ab6]

    Blitter* out = h.blitter();
    ImageSlotTable* images = h.images();
    const bool drawing = out != nullptr && images != nullptr;

    // 1.3 The images at the temporary Size delta + 1 [BYTES seg10:0x9ada-0x9ade] (claim 7).
    w.size = add16(d, 1);
    if (drawing) {
        if (!reload) {
            // Every slot but the icon backup 0x20; the skip of 0x27 < k < 0x30 at Size 2 needs
            // the reload flag and so never applies [BYTES seg10:0x9ae7-0x9b26 seg10:0x9b10] (claim 8).
            for (int k = 0; k < 0x3a; ++k) {
                if (k == kSlotIconBackup) continue;
                rescale_image(*out, *images, k, kDcNone, -2, w.size, w.w740);
            }
        } else {
            // The icon flame images 14 + i, i below pool 0's allocated count [BYTES
            // seg10:0x9b28-0x9b4b] (claim 9).
            const std::int16_t end = add16(w.pools.allocated(0), 0xe);
            for (int k = 0xe; k < end; ++k)
                rescale_image(*out, *images, k, kDcNone, -2, w.size, w.w740);
        }
        // The fragments' bitmaps DGROUP:0x4e94 + 2i, i < DGROUP:0x9408 [BYTES seg10:0x9b4d-0x9b80]
        // (claim 10).
        for (int i = 0; i < w.pools.in_use(1); ++i) {
            DcId* f = h.fragment_word(i);
            if (f == nullptr) {
                // A word the port does not hold reads 0: no bitmap [DESIGN ADR 0016 decision 2].
                guard_todo(g_once_fragment, "a fragment bitmap word the port does not hold", "seg10:0x9b4d");
                continue;
            }
            if (*f != kDcNone) *f = rescale_image(*out, *images, kRescaleSlotNone, *f, -1, w.size, w.w740);
        }
    }
    // The new Size, slot 0x22 freed and seg10:0x5c42 [BYTES seg10:0x9b82-0x9b9d] (claim 11).
    w.size = new_size;
    w.shift = sub16(4, new_size);
    ss.refused = false;  // the Size no longer stands in for a refused INI value (size_from_ini)
    if (drawing) free_image_slot(*out, *images, kSlotFlameCells);
    h.flame_cells();
    if (drawing) h.images_changed();  // the port's copies of the slot words [DESIGN]

    // 1.4 The size words of pools DGROUP:0x9326 ... DGROUP:0x9324, every allocated record [BYTES
    // seg10:0x9ba0-0x9cb5] (claim 12). The range starts at 1 in a Demon's Gate wave.
    for (int p = w.roles.radar_first; p <= w.roles.ships; ++p) {
        if (p < 0 || p >= kPoolCount) {
            guard_todo(g_once_pool, "a pool outside DGROUP:0x1df8", "seg10:0x9ba0");  // [DESIGN ADR 0016 decision 2]
            continue;
        }
        for (int r = 0; r < w.pools.allocated(p); ++r) {
            if (r >= w.pools.records(p)) {
                guard_todo(g_once_record, "a pool record outside its block", "seg10:0x9bac");
                break;
            }
            rescale_object_words(w.pools.rec(p, r), p, w.roles.ships, d, new_size, enhance, deep);
        }
    }

    // 1.5 The ship block [BYTES seg10:0x9cbf-0x9cd3] (claim 15).
    if (reload || (new_size == 2 && enhance != 0)) {
        if (drawing)  // the weapon images and the fruit [BYTES seg10:0x9cd7-0x9cf0] (claim 16)
            for (int k = 0x30; k < 0x3a; ++k) free_image_slot(*out, *images, k);
        for (int i = 0; i <= w.last_ship; ++i) {  // [BYTES seg10:0x9dae-0x9db4]
            if (i >= kShipCount || i >= w.pools.records(w.roles.ships)) {
                guard_todo(g_once_ship, "a ship index outside the ship records", "seg10:0x9db8");  // [DESIGN ADR 0016 decision 2]
                break;
            }
            if (drawing) free_image_slot(*out, *images, kShipImageSlotBase + i);  // [BYTES seg10:0x9db8-0x9dc6]
            ShipRecord& s = w.ship(i);
            // The wing bytes before the render, `cbw` [BYTES seg10:0x9dc9-0x9ddb].
            const std::int16_t wing0 = sbyte_at(s, 0xac), wing1 = sbyte_at(s, 0xad);
            // "Loading Ship # " + i is built in [bp - 0x4a] and passed to nothing [BYTES
            // seg10:0x9dde-0x9e08] (claim 18).
            h.ship_sheet(i);  // seg14:0x11a8(i) with Quick 0 [BYTES seg10:0x9e0d-0x9e15]
            ObjectRecord& o = w.ship_object(i);
            std::int16_t side = 0, gap = 0;
            if (s.roster_index == -1) {
                // The object's words as seg14:0x11a8 left them [BYTES seg10:0x9cfa-0x9d25] (claim 17,
                // O12: the twice-scaled case of the shared branch stays as the bytes do it).
                side = o.size;
                gap = o.gap;
            } else if (const RosterRecord* rr = roster_of(w, i)) {
                // Roster bytes +0x7a and +0x7b, `cbw` [BYTES seg10:0x9e16-0x9e43].
                side = static_cast<std::int8_t>(rr->size);
                gap = static_cast<std::int8_t>(rr->gap);
            } else {
                guard_todo(g_once_roster, "a roster index the port does not hold", "seg10:0x9e1f");  // reads 0 [DESIGN ADR 0016 decision 2]
            }
            // Design word +0x2a (DGROUP:0x96fc + 0xad·n) [BYTES seg10:0x9d1d seg10:0x9e3a].
            const int n = s.design_index;
            if (n < 0 || n >= kShipCount)
                guard_todo(g_once_design, "a design index the port does not hold", "seg10:0x9d1d");  // the write is lost [DESIGN ADR 0016 decision 2]
            DesignWordSet dw = design_words(n);
            dw.w2a = side;
            set_design_words(n, dw);
            w.restored = 1;  // DGROUP:0x92e4 [BYTES seg10:0x9d28]
            o.size = size_scaled(side, new_size);  // [BYTES seg10:0x9d2e-0x9d48]
            o.gap = size_scaled(gap, new_size);    // [BYTES seg10:0x9d5b-0x9d66]
            o.src_y = o.gap;                       // [BYTES seg10:0x9d77-0x9d7b]
            if (wing0 == 0) h.wing_cut(i, 0);      // [BYTES seg10:0x9d7f-0x9d8f]
            if (wing1 == 0) h.wing_cut(i, 1);      // [BYTES seg10:0x9d92-0x9da2]
            w.restored = 0;                        // [BYTES seg10:0x9da5]
        }
        // The hazard slots and their cache words DGROUP:0x381a, 0x381c, 0x381e + 6k = 0xffff [BYTES
        // seg10:0x9e46-0x9e72] (claim 19). The port holds no cache (M5): logged once.
        if (drawing)
            for (int k = 0x17; k < 0x1c; ++k) free_image_slot(*out, *images, k);
        log_once(h, g_once_cache, "the hazard image cache DGROUP:0x381a (M5)", "seg10:0x9e58");
        // The pool-8 kind-2 entries [BYTES seg10:0x9e74-0x9f5c] (claim 20, O3).
        for (int j = 0; j < w.pools.in_use(8); ++j) {
            if (j >= kPool8Max) {
                guard_todo(g_once_pool8, "a pool-8 entry beyond DGROUP:0x3d50's 80", "seg10:0x9f42");
                break;
            }
            Pool8Entry& e = w.pool8[static_cast<std::size_t>(j)];
            if (e.at(0x1c) != 2) continue;  // DGROUP:0x3d6c [BYTES seg10:0x9f42-0x9f4b]
            const std::int16_t cls = e.at(0x30);  // DGROUP:0x3d80
            if (cls <= 2) {
                // Kind 2 becomes 1 (signed test) [BYTES seg10:0x9f4d-0x9f56]; whether a kind-2 record
                // can have class <= 2 is [OPEN] (M5), so the case is logged once.
                e.at(0x1c) = 1;
                log_once(h, g_once_kind, "a kind-2 pool-8 entry of class <= 2 (M5)", "seg10:0x9f56");
                continue;
            }
            if (cls < 5) continue;  // classes 3 and 4 keep their images [BYTES seg10:0x9e80-0x9e85]
            // The class row DGROUP:0x4d90 + 0x14·cls (World::classes, 13 rows); a row the port
            // does not hold reads 0 [DESIGN ADR 0016 decision 2].
            static const std::array<std::uint8_t, 0x14> kNoRow{};
            const bool held = cls < static_cast<int>(w.classes.size());
            if (!held) guard_todo(g_once_class, "a hazard class beyond DGROUP:0x4d90's rows", "seg10:0x9e89");
            const auto& row = held ? w.classes[static_cast<std::size_t>(cls)] : kNoRow;
            const int mirror = row[3] == 0xff ? 1 : 0;  // byte DGROUP:0x4d93 [BYTES seg10:0x9e89-0x9e9c]
            // seg10:0x18b0(+0x22, +0x20, +0x2a, mirror, cls), before the size stores, so it sees
            // the scaled pool words [BYTES seg10:0x9ea1-0x9ec2] (O3).
            const std::int16_t r = h.hazard_image(e.at(0x22), e.at(0x20), e.at(0x2a), mirror, cls, e.at(0x1e));
            if (j < w.pools.records(8)) {
                ObjectRecord& P = w.pools.rec(8, j);  // far DGROUP:0x1e18 + 0x70·j
                P.gap = size_scaled(static_cast<std::int8_t>(row[9]), new_size);   // byte 0x4d99 [BYTES seg10:0x9ec5-0x9edf]
                P.size = size_scaled(static_cast<std::int8_t>(row[8]), new_size);  // byte 0x4d98 [BYTES seg10:0x9ee3-0x9ef9]
                // extent = the scaled side << 3, where the placement stores the unscaled side << 3
                // (seg9:0x1c9b-0x1caa): a fault of the original the port keeps (ruling R1, claim 21)
                // [BYTES seg10:0x9efd-0x9f0c].
                P.extent = shl16(P.size, 3);
            } else {
                guard_todo(g_once_record, "a pool record outside its block", "seg10:0x9edb");
            }
            // seg10:0x533a(word DGROUP:0x3d5a) when > 0 and its weapon's word +0x0a (DGROUP:0x2f5e +
            // 0x50·w) != 0 [BYTES seg10:0x9f10-0x9f2b].
            const std::int16_t wpn = e.at(0x0a);
            if (wpn > 0 && missile_state().words.word(wpn, 0x0a) != 0) h.weapon_image(wpn);
            e.at(0x1e) = r;  // DGROUP:0x3d6e [BYTES seg10:0x9f2c-0x9f33]
        }
    }

    // 1.6 The reload path [BYTES seg10:0x9f5e-0x9fd7] (claims 22, 23).
    if (reload) {
        // seg10:0x9568: each free seg10:0x552(k, 1), then its builder, in the bytes' order [BYTES
        // seg10:0x9572-0x9668]. Size is 1 here, so the builders' own seg10:0x6308 calls return at once.
        auto free_slot = [&](int k) {
            if (drawing) free_image_slot(*out, *images, k);
        };
        free_slot(1), free_slot(2);
        h.gate_images();  // [BYTES seg10:0x958b]
        free_slot(0x16);
        h.logo_cells();   // [BYTES seg10:0x959d]
        free_slot(4);
        h.tank_image();   // [BYTES seg10:0x95ae]
        free_slot(0xc);
        h.smoke_image();  // [BYTES seg10:0x95be]
        free_slot(5);
        h.logo_strip();   // [BYTES seg10:0x95d0]
        free_slot(6), free_slot(8), free_slot(7), free_slot(9);
        h.rock_images();  // [BYTES seg10:0x9605]: clears DGROUP:0x740 (seg10:0x3aa1)
        free_slot(0xa), free_slot(0xb);
        h.effect_images();  // [BYTES seg10:0x9621]
        free_slot(0x1d);
        h.pod_image();    // [BYTES seg10:0x9631]
        free_slot(3);
        h.hook_sheet();   // [BYTES seg10:0x9641]
        free_slot(0);
        h.def_image();    // seg10:0x464a [BYTES seg10:0x9644-0x9651]
        free_slot(0x1e);  // on every call (verified claim 23, CORRECTED) [BYTES seg10:0x9654-0x965d]
        if (w.kind == 6) h.race_image();  // [BYTES seg10:0x9660-0x9668]
        // The icon strip, unscaled, with each cell from the backup [BYTES seg10:0x9f68-0x9fb1].
        free_slot(0xd);
        h.icon_strip();
        if (drawing)
            for (int i = 0; i < w.pools.allocated(0); ++i)
                out->bit_blt(slot_word(*images, 0xd), add16(mul16(lo16(i), 0x26), 6), 6, 0x20, 0x20,
                             slot_word(*images, kSlotIconBackup), shl16(lo16(i), 5), 0, kSrcCopy);
        h.icon_overlays();  // seg9:0x94ba [BYTES seg10:0x9fb3]
        for (int i = 0; i < w.pools.allocated(0); ++i) h.icon_crack(i);  // [BYTES seg10:0x9fbf-0x9fd7]
    }

    // 1.7 The design words and flame strips of every ship [BYTES seg10:0x9fd9-0xa078] (claims
    // 24-26). +0x00 and +0x02 are never read before seg14:0x0 or the shop rewrite them (claim 25).
    for (int i = 0; i <= w.last_ship; ++i) {
        if (i >= kShipCount) {
            guard_todo(g_once_ship, "a ship index outside the ship records", "seg10:0xa05b");
            break;
        }
        const int n = w.ship(i).design_index;
        // A design index the port does not hold reads 0 and its writes are lost, as in the ship
        // block (M4 s8 fixer, review L7) [DESIGN ADR 0016 decision 2].
        if (n < 0 || n >= kShipCount)
            guard_todo(g_once_design, "a design index the port does not hold", "seg10:0x9fe0");
        DesignWordSet dw = design_words(n);
        std::int16_t* w04 = h.design_word_04(n);
        if (w04 == nullptr)
            log_once(h, g_once_design04, "design word +0x04 the port does not hold", "seg10:0xa01d");
        const RescaledDesign rd = rescaled_design(dw.w2a, w04 != nullptr ? *w04 : std::int16_t{0}, new_size);
        dw.w00 = rd.w00;
        dw.w02 = rd.w02;
        set_design_words(n, dw);
        if (w04 != nullptr) *w04 = rd.w04;
        h.barrel_table(n);  // seg10:0x1502(n) [BYTES seg10:0xa039-0xa047]
        h.flame_strip(i);   // seg10:0x5d3c(i) [BYTES seg10:0xa048-0xa04f]
    }

    // 1.8 INI, the dialog, the screen [BYTES seg10:0xa07b-0xa10b] (claim 27).
    h.write_ini("Size", new_size);
    if (w.player_game != 0) h.write_ini("SetSize", new_size);  // a demo writes `Size` only [BYTES seg10:0xa0a2]
    if (ss.dialog != 0) {
        h.close_dialog();  // DGROUP:0x8f64 = 0, DestroyWindow [BYTES seg10:0xa0c6-0xa0d0]
        ss.dg_9518 = 0;    // [BYTES seg10:0xa0d5]
        ss.dialog = 0;     // [BYTES seg10:0xa0e4]
        h.post_main(0x978);  // [BYTES seg10:0xa0db-0xa0ea]
    }
    if (out != nullptr) out->pat_blt(kDcWindow, 0, 0, w.W, w.H, kBlackness);  // [BYTES seg10:0xa0ef-0xa103]
    if (drawing) h.images_changed();
    ss.quick = saved;  // [BYTES seg10:0xa108-0xa10b]
}

// ---- seg10:0x7f88

void size_reread(World& w, RescaleHost& h) {
    const std::int16_t old = w.size;  // [bp - 4] [BYTES seg10:0x7f95-0x7f98]
    const std::int16_t ini = lo16(h.ini_size());  // [bp - 6] [BYTES seg10:0x7f9b-0x7fae]
    // While Size 1 stands in for a refused INI value (size_from_ini, ruling R2), DGROUP:0x9156 is
    // that value in the original, so the same INI value is no difference there; the port's Size 1
    // must not make one (M4 s8 fixer, review M3) [DESIGN ADR 0016 decision 2]. Any other value
    // compares with the port's Size as the bytes compare it.
    const SizeSession& ss = size_session();
    if (ss.refused && ini == ss.refused_ini) return;
    if (ini == old) return;  // [BYTES seg10:0x7fb1-0x7fb4]
    // No sheet in slot 0x28 (DGROUP:0x3a38): DGROUP:0x92e2 = DGROUP:0x103c = 1 [BYTES
    // seg10:0x7fb8-0x7fbd seg10:0x803c-0x8042].
    ImageSlotTable* images = h.images();
    if (images == nullptr || slot_word(*images, kShipImageSlotBase) == kDcNone) {
        size_session().dg_92e2 = 1;
        h.set_size_reread();
        return;
    }
    // GetDC(DGROUP:0x16) into DGROUP:0x13dc with SelectPalette, and ReleaseDC after: the window DC
    // the port keeps [DESIGN ADR 0013]; the restore's stale saved word (O9) selects nothing [DESIGN
    // ADR 0016 decision 1] [BYTES seg10:0x7fbf-0x7fde seg10:0x8013-0x8034].
    // One step for a difference of exactly +-1, two of the same sign for any other (claim 36)
    // [BYTES seg10:0x7fe1-0x8012].
    if (ini < old) {
        if (sub16(old, ini) != 1) rescale_step(w, -1, h);
        rescale_step(w, -1, h);
    } else {
        if (sub16(ini, old) != 1) rescale_step(w, 1, h);
        rescale_step(w, 1, h);
    }
}

// ---- seg9:0x23e4-0x2418 and seg9:0xd38-0xd5c

void load_size_reread(World& w, RescaleHost& h) {
    SizeSession& ss = size_session();
    if (ss.dg_92e2 != 0) {  // [BYTES seg9:0x23e4-0x23e9]
        // GetPrivateProfileInt("Size", 1) into DGROUP:0x9156 and DGROUP:0x9150 = 4 - Size [BYTES
        // seg9:0x23eb-0x2408].
        w.size = size_from_ini(h.ini_size(), "seg9:0x23f9");
        w.shift = sub16(4, w.size);
        // seg10:0x522(1): seg10:0x552(k, 1) for k = 0 ... 0x39 [BYTES seg9:0x240b-0x2412
        // seg10:0x52f-0x548].
        Blitter* out = h.blitter();
        ImageSlotTable* images = h.images();
        if (out != nullptr && images != nullptr) {
            for (int k = 0; k < 0x3a; ++k) free_image_slot(*out, *images, k);
            h.slots_freed();
        }
        h.flame_cells();  // seg10:0x5c42 [BYTES seg9:0x2413]
    }
    ss.dg_92e2 = 0;  // [BYTES seg9:0x2418]
}

void new_game_logo(World& w, RescaleHost& h) {
    if (w.size == 1) return;  // [BYTES seg9:0xd38-0xd3d]
    Blitter* out = h.blitter();
    ImageSlotTable* images = h.images();
    if (out != nullptr && images != nullptr) free_image_slot(*out, *images, 5);  // [BYTES seg9:0xd3f-0xd48]
    h.logo_strip();  // seg10:0x39a8(1) [BYTES seg9:0xd4b-0xd52]
    if (out != nullptr && images != nullptr) free_image_slot(*out, *images, 0x16);  // [BYTES seg9:0xd53-0xd5c]
}

}  // namespace is::world
