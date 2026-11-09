// owner: M4 stage 3, track VISIT.
// The landing scene's GDI work (visit_win32.h). INSPACE.EXE addresses; the verdicts are
// work/analysis/m4/s3-visit-verify.md's.
#ifdef _WIN32
#include "hud/visit_win32.h"

#include <algorithm>
#include <cstdint>

#include "desk/scene_gdi.h"
#include "gfx/launch_ship.h"
#include "gfx/pad.h"
#include "hud/canvas.h"
#include "hud/gdi_canvas.h"
#include "world/records.h"

namespace is::hud {

namespace {
constexpr std::int16_t lo16(std::int32_t v) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(v) & 0xffffu));
}
constexpr std::int16_t idiv16(std::int32_t n, std::int32_t d) {
    return lo16(d) == 0 ? std::int16_t{0} : lo16(static_cast<std::int32_t>(lo16(n)) / lo16(d));
}
// cwd; sub ax, dx; sar ax, 1.
constexpr std::int16_t half16(std::int32_t v) {
    const std::int16_t w = lo16(v);
    return static_cast<std::int16_t>((w - (w < 0 ? -1 : 0)) >> 1);
}
constexpr COLORREF palettergb(int r, int g, int b) { return 0x02000000u | RGB(r, g, b); }

std::wstring wide1252(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
}  // namespace

std::function<int(dialogs::AmbulanceEnv&)>& ambulance_dialog_slot() {
    static std::function<int(dialogs::AmbulanceEnv&)> f;
    return f;
}

VisitGdi::~VisitGdi() {
    if (font_) DeleteObject(font_);
}

HDC VisitGdi::screen() const {
    gfx::Gfx* x = gfx_();
    return x && !x->surface().empty() ? x->dc() : nullptr;
}

// ---- seg15:0x302e / seg15:0x3590 / seg15:0x374c ---------------------------------------------------
// seg15:0x302e (s6-scene.md section 2; verified claims 53-58).
std::optional<world::LandingSlots> VisitGdi::landing_scene(const world::VisitLayout& l, const std::string& slow_text,
                                                           bool black, bool render) {
    gfx::Gfx* x = gfx_();
    HDC s = screen();
    if (black && s) PatBlt(s, 0, 0, l.W, l.H, BLACKNESS);  // DGROUP:0x93a6 = 0 [BYTES seg15:0x3079-0x3094]
    if (x) x->palette_save(0, 1);                          // seg10:0x6da6(0, 1) [BYTES seg15:0x3099-0x309d]
    // seg10:0x94c4(0, 6, 0x0000FF00, W/2, DGROUP:0x90ec/2, text): the play height, not the client
    // height l.H (review L1) [BYTES seg15:0x30e3-0x3148].
    if (!slow_text.empty() && x)
        x->font_text(0, 6, 0x0000FF00, idiv16(l.W, 2), idiv16(play_height ? play_height() : l.H, 2), slow_text);
    incoming_ = 0;
    // Step 5's DGROUP:0x92d6 = trunc(22 H / 100) (seg5:0x110) and DGROUP:0x92d8 = H - 10 [BYTES
    // seg15:0x314b-0x318f]; DGROUP:0x92de is the scene's slot 0 bottom + 1 (step 7), the client
    // height without a scene [DESIGN] (M4 s6 PAD, for the jet).
    d92d6 = lo16(22 * l.H / 100);
    d92d8 = lo16(l.H - 10);
    d92de = l.H;
    std::optional<world::LandingSlots> out;
    if (render && x && scene) {  // DGROUP:0x928 [BYTES seg15:0x3192-0x3199]
        // seg10:0x9526(out, handle, 105, 0, 0, 0, 0, W, H, 0, 0, 0) [BYTES seg15:0x31e0-0x320b]
        const desk::Render r = scene(105, l.W, l.H);
        scene_img_ = x->create_image(l.W, l.H);
        desk::draw_scene(*x, scene_img_->hdc(), r, todo);
        if (s) BitBlt(s, 0, 0, l.W, l.H, scene_img_->hdc(), 0, 0, SRCCOPY);  // [BYTES seg15:0x34fc-0x3519]
        if (r.slots.size() >= 2) {
            // out +6, out +0xa [BYTES seg15:0x3248-0x325a]; step 7's DGROUP:0x92de and DGROUP:0x92d6
            // [BYTES seg15:0x34f9] (M4 s6 PAD, binding 10).
            out = world::LandingSlots{r.slots[0].b, r.slots[1].t};
            d92de = out->bottom;
            d92d6 = lo16(out->top - 40);
        }
    }
    // Step 9: seg15:0x1a42, the ship and the jet's frames, with or without the fast flag [BYTES
    // seg15:0x3192-0x3199 seg15:0x3547] (M4 s6 PAD).
    jet_frames(l);
    return out;
}

// seg15:0x3caa(first) in state 6 (s6-scene.md section 3; s6-scene-verify.md claims 61-68): the jet
// from y = DGROUP:0x92d8 up past DGROUP:0x92d6 through frames 0 ... 15 and down again with frame
// 15, composited in the work DC over the scene DC's clipped copy, its shadow at depth <= 8 (ruling
// R2), the ordinal-14 fade at each index change; 1 when it has landed [BYTES seg15:0x3cc6-0x3d5a
// seg15:0x3d60-0x3e44 seg15:0x3e47-0x3eb5 seg15:0x3f57-0x3f6a seg15:0x3f6d-0x3fda seg15:0x3fdf-0x4043
// seg15:0x4046-0x40d0 seg15:0x40d5-0x4141 seg15:0x4144-0x4219 seg15:0x42ac-0x42de seg15:0x42e1-0x4362
// seg15:0x43b3-0x4468 seg15:0x446d-0x451a seg15:0x451f-0x4540]. The fast flag's return at its entry
// and its sound 0x1c are the visit's (world/visit.cpp). 1 at once without frames [DESIGN].
int VisitGdi::incoming(int first) {
    gfx::Gfx* g = gfx_();
    HDC s = screen();
    Jet& j = jet_;
    if (!g || !s || !frames_ || !frames_mask_ || tab_[0].w <= 0) return 1;
    const int W = g->logical_width();
    const Frame& f0 = tab_[0];
    if (first != 0) {
        j.y10 = lo16(10 * d92d8);
        j.phase = 0;
        j.clip = {0, 0, W, d92de};                 // DGROUP:0x3cb0 ... 0x3cb6
        work_ = g->create_image(f0.w, f0.h + 15);  // seg15:0x3bfa(frame 0's w, h + 15)
        j.step = idiv16(d92d8 - d92d6, 16);
        j.speed = 150;
        j.idx = 0;
        j.level = 100;
        j.started = true;
    }
    if (!j.started || !work_) return 1;
    if (j.step == 0) j.step = 1;  // [BYTES seg15:0x3d60]
    HDC w = work_->hdc();
    const int x0 = half16(W) - half16(f0.w);
    // PatBlt BLACKNESS over the rectangle DGROUP:0x1814 ... 0x181a, then IntersectRect with the
    // clip and the part from the scene DC at (r.left, r.top - DGROUP:0x3cb2) [BYTES
    // seg15:0x3f6d-0x3fda seg15:0x434b-0x43ae].
    const auto compose = [&](int top) {
        const RECT r{x0, top, x0 + f0.w, top + f0.h + 15};
        PatBlt(w, 0, 0, r.right - r.left, r.bottom - r.top, BLACKNESS);
        RECT c{};
        if (scene_img_ && IntersectRect(&c, &r, &j.clip))
            BitBlt(w, c.left - r.left, c.top - r.top, c.right - c.left, c.bottom - c.top, scene_img_->hdc(), c.left,
                   c.top - j.clip.top, SRCCOPY);
        return r;
    };
    // The idx frame's mask through DGROUP:0x5eaa at (half(w0) - half(w_idx) + fx, fy) [BYTES
    // seg15:0x4050-0x40c4 seg15:0x43b3-0x445c] (verified claim 65).
    const auto shadow = [&](int h) {
        const Frame& fi = tab_[static_cast<std::size_t>(j.idx)];
        const int fx = j.idx < 10 ? 25 - 2 * j.idx : 15 - j.idx;
        const int fy = j.idx < 10 ? 4 * (15 - j.idx) : 30 - j.idx;
        gfx::shadow_blit(w, half16(f0.w) - half16(fi.w) + fx, fy, fi.w, h, frames_mask_->hdc(), 0, fi.y);
    };
    const auto frame = [&](const Frame& f, int y) {  // SRCAND, SRCPAINT (verified claim 64)
        BitBlt(w, half16(f0.w) - half16(f.w), y, f.w, f.h, frames_mask_->hdc(), 0, f.y, SRCAND);
        BitBlt(w, half16(f0.w) - half16(f.w), y, f.w, f.h, frames_->hdc(), 0, f.y, SRCPAINT);
    };
    if (j.phase == 1) {  // [BYTES seg15:0x42e1-0x4540] (verified claim 67)
        j.speed = std::max(j.speed - 5, 50);
        const RECT r = compose(idiv16(j.y10, 10));
        shadow(tab_[static_cast<std::size_t>(j.idx)].h);  // the idx frame's own height here [BYTES seg15:0x4442]
        frame(tab_[15], 2);
        BitBlt(s, r.left, r.top, r.right - r.left, r.bottom - r.top, w, 0, 0, SRCCOPY);
        j.y10 = lo16(j.y10 + idiv16(lo16(2 * j.speed), 20));
        return j.y10 > lo16(10 * (d92d6 + 44)) ? 1 : 0;
    }
    const int old = j.idx;
    j.idx = std::clamp<int>(idiv16(idiv16(j.y10, -10) + d92d8 - 55, j.step), 0, 15);  // (verified claim 63)
    const int y = idiv16(j.y10, 10);
    if (first != 0) j.prevy = y;
    if (old < j.idx && j.phase == 0) {  // the fade [BYTES seg15:0x3e47-0x3eb5] (verified claim 68)
        int lv0 = j.level;
        j.level = idiv16(lo16(-100 * j.idx), 30) + 100;
        if (lv0 == 0) lv0 = 1;
        const int p = idiv16(lo16(100 * j.level), lv0);
        g->palette_fade(3, p);  // seg10:0x993a, ordinal 14
        g->palette_fade(2, idiv16(p + 200, 3));
    }
    j.speed = std::max(150 - 4 * j.idx, 9);  // [BYTES seg15:0x3f57-0x3f6a]
    const RECT r = compose(y);
    {
        const Frame& fi = tab_[static_cast<std::size_t>(j.idx)];
        const int fy = j.idx < 10 ? 4 * (15 - j.idx) : 30 - j.idx;
        shadow(std::min(f0.h - (j.prevy - y) - fy + 14, fi.h));
    }
    frame(tab_[static_cast<std::size_t>(j.idx)], 0);
    BitBlt(s, r.left, r.top, r.right - r.left, r.bottom - r.top, w, 0, 0, SRCCOPY);
    j.prevy = y;
    j.y10 = lo16(j.y10 + idiv16(lo16(2 * j.speed), -3));  // [BYTES seg15:0x42ac-0x42de] (verified claim 66)
    if (j.y10 < lo16(10 * d92d6)) {
        j.y10 = lo16(j.y10 - 20);
        j.speed = 200;
        j.phase = 1;
    }
    return 0;
}

// seg15:0x1a42 in state 6 [BYTES seg15:0x1a4f-0x1e7a]: seg15:0x20ca, the table of gfx/launch_ship.h
// jet_table, DGROUP:0x3ccc = frame 0's width x the heights' sum, black, COLORONCOLOR, each frame
// seg10:0x9962 at x 0 from DGROUP:0x92b2's (0, 0, k sz, k sz) SRCCOPY, then the mask DGROUP:0x3cce
// [BYTES seg15:0x1ca8-0x1e7a]. The fast flag DGROUP:0x928 skips the stretches only, and no frame is
// shown then, so the port draws them [DESIGN]. An unknown `Detail` leaves no frames [DESIGN].
void VisitGdi::jet_frames(const world::VisitLayout& l) {
    (void)l;
    build_ship();
    free_frames();
    gfx::Gfx* g = gfx_();
    if (!g || !square_ || sz_ <= 0) return;
    const std::array<gfx::JetFrame, 16> t = gfx::jet_table(sz_, ship_look ? ship_look().detail : 3);
    const int total = gfx::jet_table_height(t);
    if (t[0].w <= 0 || total <= 0) return;
    for (std::size_t i = 0; i < t.size(); ++i) tab_[i] = {t[i].w, t[i].h, t[i].y};
    frames_ = g->create_image(t[0].w, total);
    HDC d = frames_->hdc();
    PatBlt(d, 0, 0, t[0].w, total, BLACKNESS);
    SetStretchBltMode(d, COLORONCOLOR);
    const int ks = lo16(gfx::kLaunchK * sz_);
    for (const Frame& f : tab_) StretchBlt(d, 0, f.y, f.w, f.h, square_->hdc(), 0, 0, ks, ks, SRCCOPY);  // [BYTES seg15:0x1db7-0x1e11]
    frames_mask_ = g->mask(*frames_);  // [BYTES seg15:0x1e24]
}

// seg15:0x20ca (gfx/launch_ship.h ship_20ca) from the player's design, ship and roster records.
void VisitGdi::build_ship() {
    gfx::Gfx* g = gfx_();
    body_.reset();
    square_.reset();
    flameless_.reset();
    sz_ = 0;
    if (!g || !ship_look) return;
    const gfx::LaunchShip sh = gfx::ship_20ca(*g, ship_look(), todo);
    body_ = sh.body;
    square_ = sh.square;
    flameless_ = sh.flameless;
    sz_ = sh.ok() ? sh.s : 0;
}

void VisitGdi::rebuild_ship() {
    body_.reset();  // DeleteObject of DGROUP:0x92b0, 0x92b2, 0x92b4, each zeroed
    square_.reset();
    flameless_.reset();
    gfx::Gfx* g = gfx_();
    if (!g || !main_ship_art) return;
    const std::optional<gfx::LaunchShipArt> art = main_ship_art();
    if (!art) return;
    const gfx::LaunchShip sh = gfx::ship_2a08(*g, *art, todo);
    body_ = sh.body;
    square_ = sh.square;
}

int VisitGdi::pad_s4(int roster_s4) {
    // GetObject(DGROUP:0x92b0) and [bp-6] = (bmHeight / k) << 2 [BYTES seg15:0x4cec-0x4d1c]; with no
    // body image GetObject fails and leaves [bp-0x36] unwritten, which the port reads as phase 0's
    // s4 [DESIGN] (ADR 0016 decision 1: the visit has no body only without a ship look).
    if (!body_) return roster_s4;
    return lo16(idiv16(body_->height(), gfx::kLaunchK) * 4);
}

void VisitGdi::free_frames() {
    frames_.reset();
    frames_mask_.reset();
    tab_ = {};
}

// seg15:0x1eac [BYTES seg15:0x1eba-0x2081] (verified claim 38): ks = k sz, s4 = 4 sz; DGROUP:0x3ccc =
// s4 x 2 s4, black, COLORONCOLOR; frame 0 at y 0 from DGROUP:0x92b2 (with the flame), frame 1 at y
// s4 from DGROUP:0x92b4 (without), both seg10:0x9962 from (0, 0, ks, ks); the mask DGROUP:0x3cce.
void VisitGdi::two_frames() {
    free_frames();
    gfx::Gfx* g = gfx_();
    if (!g || !square_ || !flameless_ || sz_ <= 0) return;
    const int s4 = lo16(4 * sz_), ks = lo16(gfx::kLaunchK * sz_);
    frames_ = g->create_image(s4, lo16(2 * s4));
    HDC d = frames_->hdc();
    PatBlt(d, 0, 0, s4, 2 * s4, BLACKNESS);
    SetStretchBltMode(d, COLORONCOLOR);
    StretchBlt(d, 0, 0, s4, s4, square_->hdc(), 0, 0, ks, ks, SRCCOPY);
    StretchBlt(d, 0, s4, s4, s4, flameless_->hdc(), 0, 0, ks, ks, SRCCOPY);
    frames_mask_ = g->mask(*frames_);
    tab_[0] = {s4, s4, 0};
    tab_[1] = {s4, s4, s4};
}

// seg15:0x3590 sub-step 0, state 6: seg10:0x6e76(-1, 1), seg10:0x6d7a(v, v, v) for v = 40 ... 224
// step 4, seg10:0x6e4c(), seg10:0x6da6(1, 0), seg10:0x6e76(0, 2), the greys again, seg10:0x6e4c(),
// seg15:0x482e, seg10:0x6e76(-1, 1), seg10:0x6dd4(1) [BYTES seg15:0x3619-0x36b1] (verified claim
// 16, CORRECTED). The pad's PALETTERGB greys are the nearest entries of the palette at the build,
// as on a 256-colour display (ADR 0007 decision 1) [DESIGN].
void VisitGdi::build_pad(const world::VisitLayout& l) {
    gfx::Gfx* g = gfx_();
    if (!g) return;
    const auto greys = [g] {
        for (int v = gfx::kVisitGreyFirst; v <= gfx::kVisitGreyLast; v += gfx::kVisitGreyStep) g->palette_colour(v, v, v);
        g->palette_animate();
    };
    g->palette_scheme(-1, 1);
    greys();
    g->palette_save(1, 0);
    g->palette_scheme(0, 2);
    greys();
    pad_geo_ = gfx::pad_geometry(play_height ? play_height() : l.H, l.H);
    pad_ = gfx::pad_image(*g, gfx::pad_picture(pad_geo_), [g](int v) { return g->colour_index(palettergb(v, v, v)); });
    g->palette_scheme(-1, 1);
    g->palette_restore(1);
}

// seg15:0x3590 sub-step 0 between seg15:0x1eac and seg15:0x374c(1) (s6-pad-verify.md claim 16,
// CORRECTED) [BYTES seg15:0x35d8-0x36b1].
void VisitGdi::descent_start(const world::VisitLayout& l, bool black, bool fade) {
    // seg15:0x1328(1): the jet's frames, the strip and the scene DC freed (not in state 8) [BYTES
    // seg15:0x35c9-0x35d0]; seg15:0x1eac; the black screen when `black` (DGROUP:0x93a6 = 0)
    // [BYTES seg15:0x35d8-0x35f3]; with `fade` (DGROUP:0x928 = 0) ordinal 14 (3, 0xcc) and (2,
    // 0x89) [BYTES seg15:0x35a3-0x3616] (s6-scene.md section 4, verified claims 69-70).
    strip_.reset();
    scene_img_.reset();
    jet_.started = false;
    two_frames();
    if (black) black_screen();
    gfx::Gfx* g = gfx_();
    if (g && fade) {
        g->palette_fade(3, 0xcc);
        g->palette_fade(2, 0x89);
    }
    // seg10:0x6e76(-1, 1), the greys, seg10:0x6e4c, seg10:0x6da6(1, 0), seg10:0x6e76(0, 2), the
    // greys, seg10:0x6e4c, seg15:0x482e, seg10:0x6e76(-1, 1), seg10:0x6dd4(1) (build_pad).
    build_pad(l);
    // seg15:0x374c(1)'s first part: the back buffer DGROUP:0x9580 (W x DGROUP:0x10, black, the pad at
    // DGROUP:0x3cb0 / 0x3cb2) that seg16:0x1f78's seg16:0x2144 draws into and then blits whole to the
    // screen and frees [BYTES seg15:0x379b-0x3859 seg16:0x2152-0x2161 seg16:0x271b-0x27d3] (verified
    // claims 44-45). The port shows its surface once per pass, so the buffer's copy first and
    // seg16:0x2144 on the screen after it show the same picture [DESIGN].
    if (g && !g->surface().empty() && l.W > 0 && l.H > 0) {
        gfx::ImagePtr back = g->create_image(l.W, l.H);
        PatBlt(back->hdc(), 0, 0, l.W, l.H, BLACKNESS);
        if (pad_)
            BitBlt(back->hdc(), half16(l.W) - half16(pad_->width()), l.H - pad_->height(), pad_->width(), pad_->height(),
                   pad_->hdc(), 0, 0, SRCCOPY);
        BitBlt(g->dc(), 0, 0, l.W, l.H, back->hdc(), 0, 0, SRCCOPY);
    }
    redraw(l, 1, 0, true);
}

void VisitGdi::draw_pad(HDC d, int x, int y, int w, int h) {
    // The bare pad from DGROUP:0x92b6 [BYTES seg16:0x260c-0x2716] (verified claim 67).
    if (!pad_ || w <= 0 || h <= 0) return;
    BitBlt(d, x, y, w, h, pad_->hdc(), 0, 0, SRCCOPY);
}

// seg15:0x374c's drawing on every call, state 6 (s6-pad.md 3.2) [BYTES seg15:0x39b6-0x3b92]
// (verified claims 46-49): the ship rectangle (half(W) - half(s4), y, + s4, + s4 + 40); the work DC
// seg15:0x3bfa(s4, s4 + 40) blacked; the pad inside the rectangle from DGROUP:0x92b6; the shadow while
// hgt = DGROUP:0x92d6 - y is below 120, from frame 1's mask at (0, half(hgt) + 9) through
// DGROUP:0x5eaa (ruling R2); frame `flag` (0 with the flame, 1 without) at (0, 5) through its mask;
// the work DC to the screen. DGROUP:0x92d6 is the descent's end, H - pad_c - 0x34 - s4 + pad_c - 10
// (world/visit.cpp descent_step) [BYTES seg15:0x385e-0x3870].
void VisitGdi::descent_frame(const world::VisitLayout& l, int y, int s4, int flag) {
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g || s4 <= 0) return;
    if (!work_ || work_->width() != s4 || work_->height() != s4 + 40) work_ = g->create_image(s4, s4 + 40);
    HDC w = work_->hdc();
    const int left = half16(l.W) - half16(s4);
    const RECT ship{left, y, left + s4, y + s4 + 40};
    PatBlt(w, 0, 0, s4, s4 + 40, BLACKNESS);
    const int pl = half16(l.W) - half16(l.pad_w), pt = l.H - l.pad_c - 0x34;
    const RECT pad{pl, pt, pl + l.pad_w, pt + l.pad_c + 0x34};  // DGROUP:0x3cb0 ... 0x3cb6
    RECT i{};
    if (pad_ && IntersectRect(&i, &pad, &ship))
        BitBlt(w, i.left - ship.left, i.top - ship.top, i.right - i.left, i.bottom - i.top, pad_->hdc(),
               i.left - pad.left, i.top - pad.top, SRCCOPY);
    if (frames_ && frames_mask_) {
        const int end = lo16(pt - s4 + l.pad_c - 10);
        const int hgt = lo16(end - y);
        if (hgt < 0x78) gfx::shadow_blit(w, 0, half16(hgt) + 9, s4, s4, frames_mask_->hdc(), 0, tab_[1].y);
        const int fy = tab_[flag != 0 ? 1 : 0].y;
        BitBlt(w, 0, 5, s4, s4, frames_mask_->hdc(), 0, fy, SRCAND);
        BitBlt(w, 0, 5, s4, s4, frames_->hdc(), 0, fy, SRCPAINT);
    }
    BitBlt(s, ship.left, ship.top, s4, s4 + 40, w, 0, 0, SRCCOPY);
}

// ---- seg16:0x2144's panels [BYTES seg16:0x2152-0x231a] ----------------------------------------------
void VisitGdi::panels(HDC s, const world::VisitLayout& l) {
    gfx::Gfx* g = gfx_();
    GdiCanvas* cv = canvas_();
    if (!g) return;
    HBRUSH metal = cv ? cv->metal_brush() : nullptr;  // DGROUP:0x500a
    const int pw = l.panel_w, bh = l.panel_bg_h;      // DGROUP:0x6dac (= 0x3cac), DGROUP:0x6dae
    const auto fill = [&](int x) {                     // PatBlt(x, DGROUP:0x6db2 (= ry), pw, bh, PATCOPY)
        HGDIOBJ ob = SelectObject(s, metal ? static_cast<HGDIOBJ>(metal) : GetStockObject(GRAY_BRUSH));
        PatBlt(s, x, l.ry, pw, bh, PATCOPY);
        SelectObject(s, ob);
    };
    const auto outline = [&](int x0, int x1) {         // CreatePen(PS_SOLID, 1, 0x00C0C0C0)
        HPEN pen = CreatePen(PS_SOLID, 1, g->dc_colour(0x00c0c0c0));
        HGDIOBJ op = SelectObject(s, pen);
        MoveToEx(s, x0, l.ry, nullptr);
        LineTo(s, x1, l.ry);
        LineTo(s, x1, l.ry + bh);
        LineTo(s, x0, l.ry + bh);
        SelectObject(s, op);
        DeleteObject(pen);
    };
    fill(l.rx);                                        // [BYTES seg16:0x2164-0x2194]
    g->palette_save(1, 0);                             // seg10:0x6da6(1, 0) [BYTES seg16:0x219d]
    g->palette_scheme(0, 2);                           // seg10:0x6e76(0, 2) [BYTES seg16:0x21a9]
    for (int v = 40; v < 228; v += 4) g->palette_colour(v, v, v);  // [BYTES seg16:0x21b1-0x21d0]
    g->palette_animate();                              // seg10:0x6e4c [BYTES seg16:0x21d2]
    sloped_sides(s, l);                                // seg15:0x454c [BYTES seg16:0x21d7]
    g->palette_scheme(0, 1);                           // seg10:0x6e76(0, 1) [BYTES seg16:0x21e0]
    g->palette_restore(1);                             // seg10:0x6dd4(1) [BYTES seg16:0x21ea]
    outline(l.rx + pw, l.rx);                          // [BYTES seg16:0x21f0-0x2267]
    fill(l.lx);                                        // [BYTES seg16:0x226c-0x229e]
    outline(l.lx, l.lx + pw);                          // [BYTES seg16:0x22a3-0x231a]
}

// seg15:0x454c: the room's walls between the panels, 2-px PatBlt columns from each panel's inner
// edge toward the vanishing point (W/2, DGROUP:0x3ca2), grey 170 - 150·i/N, then four black 2-px
// pen lines to it (read by the SCENE track, M4 stage 6) [BYTES seg15:0x454c-0x482c]. seg5:0x314
// and seg5:0x110 are the long multiply and the truncating long divide. [OPEN: an implementer's
// reading that no verifier has checked (shell.md "The landing scene"); the reviewer's spot check
// agrees up to seg15:0x4689; a verify round on seg15:0x454c-0x482c settles it] (review M2).
void VisitGdi::sloped_sides(HDC s, const world::VisitLayout& l) {
    using world::lo16;
    gfx::Gfx* g = gfx_();
    if (!g) return;
    const auto mul = [](std::int32_t a, std::int32_t b) {
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
    };
    const auto div = [](std::int32_t a, std::int32_t b) { return b == 0 ? 0 : world::div32(a, b); };  // ADR 0016 decision 4 [DESIGN]
    const int pw = l.panel_w, bh = l.panel_bg_h, vy = l.vy;          // DGROUP:0x6dac, 0x6dae, 0x3ca2
    const int edge = lo16(pw + l.lx);                                 // [bp-0x44]
    const int bottom = lo16(bh + l.ry);                               // [bp-0x14]
    const int half = world::idiv16(lo16(l.W), 2);                     // W/2, `cwd; sub; sar`
    const int stop = lo16(half - 0x5a);                               // di [BYTES seg15:0x4585-0x4594]
    const std::int32_t D = lo16(half - lo16(edge + 2));               // [bp-0x3e]
    const int y1 = lo16(div(mul(lo16(vy - l.ry), lo16(D - 0x5a)), D) + l.ry);    // [BYTES seg15:0x45a5-0x45ca]
    const int y2 = lo16(div(mul(lo16(vy - bottom), lo16(D - 0x5a)), D) + bottom);  // [BYTES seg15:0x45d6-0x45f5]
    const std::int32_t N = lo16(stop - edge);                         // [bp-0x32] [BYTES seg15:0x45f7-0x45fd]
    const std::int32_t top_d = lo16(y1 - l.ry), bot_d = lo16(y2 - bottom);  // [bp-0x2a], [bp-0x2e]
    for (std::int32_t i = 0; i < N; i += 2) {                        // [BYTES seg15:0x462b-0x4655 seg15:0x4635]
        const int c = lo16(lo16(div(mul(i, -150), N)) + 0xaa) & 0xff;            // [BYTES seg15:0x4655-0x4689]
        const int top = lo16(div(mul(i, top_d), N) + l.ry);                       // [BYTES seg15:0x469c-0x46bd]
        const int h = lo16(div(mul(i, bot_d - top_d), N) + bh);                  // [BYTES seg15:0x46c0-0x46e6]
        HBRUSH b = CreateSolidBrush(g->dc_colour(0x02000000u | RGB(c, c, c)));    // PALETTERGB [BYTES seg15:0x46ec-0x46fb]
        HGDIOBJ ob = SelectObject(s, b);
        PatBlt(s, lo16(edge + i), top, 2, h, PATCOPY);                            // [BYTES seg15:0x4709-0x471a]
        PatBlt(s, lo16(l.rx - 2 - i), top, 2, lo16(h + 2), PATCOPY);              // [BYTES seg15:0x471f-0x473d]
        SelectObject(s, ob);
        DeleteObject(b);
    }
    HPEN pen = CreatePen(PS_SOLID, 2, g->dc_colour(0));              // [BYTES seg15:0x4756-0x476c]
    HGDIOBJ op = SelectObject(s, pen);
    MoveToEx(s, lo16(edge + 2), lo16(l.ry - 1), nullptr);            // [BYTES seg15:0x476f-0x4796]
    LineTo(s, half, vy);
    MoveToEx(s, lo16(edge + 2), bottom, nullptr);                    // [BYTES seg15:0x479b-0x47c2]
    LineTo(s, half, lo16(vy + 2));
    MoveToEx(s, lo16(l.rx - 2), l.ry, nullptr);                      // [BYTES seg15:0x47c7-0x47e8]
    LineTo(s, half, vy);
    MoveToEx(s, lo16(l.rx - 2), bottom, nullptr);                    // [BYTES seg15:0x47ed-0x480e]
    LineTo(s, half, vy);
    SelectObject(s, op);
    DeleteObject(pen);
}

void VisitGdi::monitor_faces(HDC s, const world::VisitLayout& l) {
    // The monitor face DGROUP:0x95a0 of seg16:0x27ea on both panels, the insides blacked [OPEN:
    // the face is a bevelled frame of ordinal 34 here. core-0007 v1-scene-complete and core-0008
    // monitor-sky-full show a light-grey bezel with a bottom strip holding a green LED and a small
    // button, about 12-14k px per monitor apart from this stand-in (compare item 3); a line-by-line
    // reading of seg16:0x27ea's drawing (548 instructions, summarised in s3-visit.md) settles it].
    gfx::Gfx* g = gfx_();
    const int t = l.T() + 3;
    for (const int x : {l.L() + 3, l.lx + 15}) {
        g->frame(x - 4, t - 4, l.mon_w + 8, l.mon_h + 8, 1);
        PatBlt(s, x, t, l.mon_w, l.mon_h, BLACKNESS);
    }
    if (monitor_) BitBlt(s, l.L() + 7, l.T() + 7, monitor_->width(), monitor_->height(), monitor_->hdc(), 0, 0, SRCCOPY);
}

void VisitGdi::status_block(const world::VisitLayout& l) {
    // [BYTES seg16:0x2448-0x25c9] (verified claim 51, CORRECTED)
    gfx::Gfx* g = gfx_();
    GdiCanvas* cv = canvas_();
    HDC s = screen();
    const VisitStatus st = status_ ? status_() : VisitStatus{};
    g->frame(l.rx + 4, l.ry + l.panel_h + 15, l.panel_w - 20, 128, 1);  // seg10:0x949c
    gfx::ImagePtr labels = cv ? cv->image(Img::kSystems) : nullptr;     // DGROUP:0x5030
    for (int k = 0; k < 5; ++k) {
        int src_y = 0;
        if (k < 3) {
            src_y = st.row[k] == -1 ? 0 : 11 * (st.row[k] + 1);
        } else if (k == 3) {
            if (st.row[3] == -1) continue;  // the label and the gauge skipped [BYTES seg16:0x24b6 seg16:0x24f5]
            const int r = st.shield ? 5 : st.cloak ? 4 : st.row[3];
            src_y = 11 * (r + 1);
        }
        if (labels)  // 11 x 32 to (rx + 8, ry + panel_h + 0x17 + 25k) [BYTES seg16:0x2530]
            BitBlt(s, l.rx + 8, l.ry + l.panel_h + 0x17 + 25 * k, 32, 11, labels->hdc(), 0, src_y, SRCCOPY);
        g->frame(l.rx + 0x39, l.ry + l.panel_h + 20 + 25 * k, l.panel_w - 0x4c, 14, 0);
        g->bar(l.bar_x(), l.bar_y(k), l.bar_w(), 10, st.value[k]);
    }
}

void VisitGdi::redraw(const world::VisitLayout& l, int phase, int saved_phase, bool left_on) {
    HDC s = screen();
    if (!s) return;
    panels(s, l);
    monitor_faces(s, l);
    status_block(l);
    // The pad strip DGROUP:0x953c while phase < 63 (and not -1 with DGROUP:0x958c >= 63), else
    // the bare pad [BYTES seg16:0x25d1-0x2650].
    const int px = l.W / 2 - l.pad_w / 2;
    const bool before_launch = phase < 63 && !(phase == -1 && saved_phase >= 63);
    // Before the launch the strip alone, nothing while DGROUP:0x953c = 0 (s6-pad-verify.md claim
    // 67) (M4 s6 PAD, binding 10).
    if (before_launch) {
        if (strip_) BitBlt(s, strip_x_, strip_y_, strip_->width(), strip_->height() - 5, strip_->hdc(), 0, 5, SRCCOPY);
    } else {
        draw_pad(s, px, l.H - l.pad_c - 0x34, l.pad_w, l.pad_c + 52);
    }
    if (left_on) left_monitor(l, limited ? limited() : true);
}

// ---- phase 2 and seg15:0x4c76 ------------------------------------------------------------------------
// Phase 2 [BYTES seg16:0xdc1-0xf32] (s6-pad.md 3.3, verified claims 51-52): frame 1 (without the
// flame) through its mask at (x, y) = (half(W) - half(s4), DGROUP:0x92d6 + 2), no pad and no erase;
// then DGROUP:0x953c: padW x (sh + 0x43), black, with the screen's padW x (sh + 0x3e) from (W/2 -
// padW/2, H - sh - 0x3e) at y 5.
void VisitGdi::ship_on_pad(const world::VisitLayout& l, int x, int y, int s4, int sh) {
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g) return;
    if (frames_ && frames_mask_ && s4 > 0) {
        BitBlt(s, x, y, s4, s4, frames_mask_->hdc(), 0, tab_[1].y, SRCAND);
        BitBlt(s, x, y, s4, s4, frames_->hdc(), 0, tab_[1].y, SRCPAINT);
    }
    if (!strip_) strip_ = g->create_image(l.pad_w, sh + 0x43);
    if (!strip_) return;
    PatBlt(strip_->hdc(), 0, 0, l.pad_w, sh + 0x43, BLACKNESS);
    strip_x_ = l.W / 2 - l.pad_w / 2;
    strip_y_ = l.H - sh - 0x3e;
    BitBlt(strip_->hdc(), 0, 5, l.pad_w, sh + 0x3e, s, strip_x_, strip_y_, SRCCOPY);
}

void VisitGdi::pad_strip(const world::VisitLayout& l, int offset, int s4) { pad_move(l, 0, 0, offset, s4); }

void VisitGdi::pad_move(const world::VisitLayout& l, int first, int down, int offset, int s4) {
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g) return;
    const int pw = l.pad_w;
    if (!strip_) strip_ = g->create_image(pw, s4 + 0x43);  // state 0's creation; the visit's is phase 2's
    if (first != 0 && down == 0)
        gfx::strip_rebuild(*g, *strip_, pw, l.pad_c, s4, pad_.get(), body_.get());  // (verified claims 55-57)
    // BitBlt(DGROUP:0x13dc, half(W) - half(padW), DGROUP:0x181c + DGROUP:0x10 - s4 - 0x43, padW, s4 +
    // 0x3e, strip, 0, 0, SRCCOPY) [BYTES seg15:0x51ed-0x520d] (verified claim 59).
    BitBlt(s, half16(l.W) - half16(pw), offset + l.H - s4 - 0x43, pw, s4 + 0x3e, strip_->hdc(), 0, 0, SRCCOPY);
}

void VisitGdi::bar(int x, int y, int w, int h, int v) {
    if (gfx::Gfx* g = gfx_()) g->bar(x, y, w, h, v);
}

// ---- seg16:0x2e3c's monitor ------------------------------------------------------------------------
void VisitGdi::monitor_begin(int font_h) {
    HDC s = screen();
    if (!s) return;
    if (!font_) {
        // CreateFont(h - 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0x22, "Arial") (verified claim 11)
        // [BYTES seg16:0x2f07-0x2f28].
        font_ = CreateFontW(font_h - 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Arial");
    }
    saved_dc_ = SaveDC(s);
    old_font_ = SelectObject(s, font_);
    SetBkColor(s, gfx_()->dc_colour(0));
    SetBkMode(s, TRANSPARENT);
    SetTextAlign(s, TA_LEFT | TA_TOP);
    SetTextColor(s, gfx_()->dc_colour(0x0000FF00));
}

void VisitGdi::monitor_end() {
    HDC s = screen();
    if (!s) return;
    if (old_font_) SelectObject(s, old_font_);
    old_font_ = nullptr;
    if (saved_dc_) RestoreDC(s, saved_dc_);
    saved_dc_ = 0;
}

int VisitGdi::text_extent(const std::string& cp1252) {
    HDC s = screen();
    if (!s || cp1252.empty()) return 0;
    HGDIOBJ of = font_ ? SelectObject(s, font_) : nullptr;
    const std::wstring w = wide1252(cp1252);
    SIZE sz{};
    GetTextExtentPoint32W(s, w.c_str(), static_cast<int>(w.size()), &sz);
    if (of) SelectObject(s, of);
    return sz.cx;
}

void VisitGdi::glyph(int x, int y, char c) {
    HDC s = screen();
    if (!s) return;
    const std::wstring w = wide1252(std::string(1, c));
    TextOutW(s, x, y, w.c_str(), static_cast<int>(w.size()));
}

void VisitGdi::black(int x, int y, int w, int h) {
    if (HDC s = screen()) PatBlt(s, x, y, w, h, BLACKNESS);
}

void VisitGdi::block(int x, int y, int w, int h, std::uint32_t colour) {
    HDC s = screen();
    if (!s) return;
    HBRUSH b = CreateSolidBrush(gfx_()->dc_colour(colour));  // CreateSolidBrush(0x0000FF80) [BYTES seg16:0x316b]
    HGDIOBJ ob = SelectObject(s, b);
    PatBlt(s, x, y, w, h, PATCOPY);
    SelectObject(s, ob);
    DeleteObject(b);
}

void VisitGdi::scroll_up(int l, int t, int r, int b, int n) {
    HDC s = screen();
    if (!s) return;
    RECT rc{l, t, r, b};
    for (int i = 0; i < n; ++i) ScrollDC(s, 0, -1, &rc, &rc, nullptr, nullptr);
}

void VisitGdi::flash(int x, int y) {
    // seg15:0x84b0 as src/desk/gdi_desk.cpp draws it: a white cross growing to 20 px, then erased
    // [BYTES seg15:0x84cd seg15:0x84d5 seg15:0x856b seg15:0x85f7].
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g) return;
    HPEN wp = CreatePen(PS_SOLID, 1, g->dc_colour(RGB(255, 255, 255)));
    HGDIOBJ op = SelectObject(s, wp);
    for (int i = 2; i < 200; ++i) {
        const int k = i / 10;
        MoveToEx(s, x, y - k, nullptr);
        LineTo(s, x, y + k);
        MoveToEx(s, x - k, y, nullptr);
        LineTo(s, x + k, y);
    }
    PatBlt(s, x - 21, y - 21, 42, 42, BLACKNESS);
    SelectObject(s, op);
    DeleteObject(wp);
}

void VisitGdi::save_monitor(int x, int y, int w, int h) {
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g || w <= 0 || h <= 0) return;
    if (!monitor_) monitor_ = g->create_image(w, h);  // DGROUP:0x953a [BYTES seg16:0x125a-0x1273]
    if (monitor_) BitBlt(monitor_->hdc(), 0, 0, w, h, s, x, y, SRCCOPY);
}

void VisitGdi::left_monitor(const world::VisitLayout& l, bool) {
    // seg16:0x64fa with DGROUP:0x9530 = 3: the record's w x h blacked at DGROUP:0x6e08, then
    // seg10:0x94c4(2, 6, 0x0000FF00, 0, w/2, y, text): the Limited edition (DGROUP:0x920c = 0)
    // 2330 at h/2 - 18 and 2331 + "..." at h/2 (verified claim 49, CORRECTED) [BYTES
    // seg16:0x6557-0x6600] [ORACLE core-0003: "ORDER FULL GAME TO SEE / WHAT APPEARS HERE...."].
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g || !lang) return;
    const int x = l.lx + 15, y = l.T() + 3;
    PatBlt(s, x, y, l.mon_w, l.mon_h, BLACKNESS);
    const bool lim = limited ? limited() : true;
    const std::string a = lim ? lang(2330) : lang(2027);
    // The full edition's second line is the subgame record's name (DGROUP:0x6da4 + 8) [OPEN M6].
    const std::string b = (lim ? lang(2331) : std::string("INNER DEMON")) + "...";
    g->font_text(2, 6, 0x0000FF00, x + l.mon_w / 2, y + l.mon_h / 2 - 18, a);
    g->font_text(2, 6, 0x0000FF00, x + l.mon_w / 2, y + l.mon_h / 2, b);
}

void VisitGdi::black_screen() {
    if (gfx::Gfx* g = gfx_())
        if (!g->surface().empty()) PatBlt(g->dc(), 0, 0, g->logical_width(), g->logical_height(), BLACKNESS);
}

void VisitGdi::free_scene() {
    strip_.reset();    // DGROUP:0x953c
    monitor_.reset();  // DGROUP:0x953a
    // The launch's step 9 (seg15:0x1328(1), seg15:0x14be, seg15:0x152e) frees the frames, DGROUP:0x92b0
    // ... 0x92b4, the flame and the pad DC [BYTES seg15:0x125b-0x12bd seg15:0x14be] (shell.md item 10);
    // the visit's copies go with the scene here [DESIGN].
    free_frames();
    body_.reset();
    square_.reset();
    flameless_.reset();
    pad_.reset();
    work_.reset();
    scene_img_.reset();
    jet_.started = false;
}

}  // namespace is::hud
#endif
