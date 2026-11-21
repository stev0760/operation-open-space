// The Hall of Justice's pictures (hall_gdi.h).
#ifdef _WIN32
#include "hall/hall_gdi.h"

#include <algorithm>

#include "desk/scene_gdi.h"
#include "world/records.h"
#include "world/rock_images.h"
#include "world/world.h"

namespace is::hall {

using world::idiv16;
using world::lo16;

namespace {
int half16(int v) { return idiv16(lo16(v), 2); }
}  // namespace

std::function<gfx::VisitShipArt()>& ship_look_slot() {
    static std::function<gfx::VisitShipArt()> f;
    return f;
}

int dead_ship_marks(HDC q, HDC q2, int k, int sz, shell::Rng& rng, const gfx::LaunchTodo& todo) {
    const std::int16_t a = lo16(lo16(sz * k) - 3);  // [bp-0xc] [BYTES seg15:0x28bf-0x28c8]
    const std::int16_t b = lo16((sz - 8) * k);      // [bp-0xa] [BYTES seg15:0x28cb-0x28d4]
    if (a == 0 || b == 0) {
        static bool once = false;  // [DESIGN ADR 0016 decision 4]
        world::guard_todo(once, "seg15:0x28b2's divisor is 0: no marks", "seg15:0x2969");
        if (todo) todo("seg15:0x28b2 skipped: a zero divisor", "seg15:0x2969");
        return 0;
    }
    // The sparse-dot brush DGROUP:0x5ea8 (CreatePatternBrush of the rows DGROUP:0x6c6, seg10:0xd8)
    // [BYTES seg10:0xe4 seg10:0xf2].
    std::uint8_t bits[16] = {};
    for (int i = 0; i < 8; ++i) bits[2 * i] = world::kMaskRows[i];
    HBITMAP bm = CreateBitmap(8, 8, 1, 1, bits);
    HBRUSH dots = bm ? CreatePatternBrush(bm) : nullptr;
    if (bm) DeleteObject(bm);
    int draws = 0;
    for (int i = 0; i < 0xfa; ++i) {  // [BYTES seg15:0x2959-0x2960]
        const int x = world::imod16(lo16(rng.rand()), a) + 3;  // [BYTES seg15:0x2963-0x296f]
        const int y = world::imod16(lo16(rng.rand()), b);      // [BYTES seg15:0x2972-0x297b]
        const int r3 = rng.rand() % 3;                         // [BYTES seg15:0x297e-0x298b]
        const int d = rng.rand() % 4 + 2;                      // [BYTES seg15:0x28de-0x28eb seg15:0x2990-0x299d]
        draws += 4;
        if (r3 == 0) {
            const HGDIOBJ o1 = dots ? SelectObject(q, dots) : nullptr;
            const HGDIOBJ o2 = dots ? SelectObject(q2, dots) : nullptr;
            PatBlt(q, x, y, d, d, 0x00A000C9);  // [BYTES seg15:0x290c-0x293b]
            PatBlt(q2, x, y, d, d, 0x00A000C9);
            if (o1) SelectObject(q, o1);
            if (o2) SelectObject(q2, o2);
        } else {
            const HGDIOBJ o1 = SelectObject(q, GetStockObject(BLACK_BRUSH));  // GetStockObject(4) [BYTES seg15:0x29a0-0x29bd]
            const HGDIOBJ o2 = SelectObject(q2, GetStockObject(BLACK_BRUSH));
            Ellipse(q, x, y, x + d, y + d);  // [BYTES seg15:0x29c5-0x29f8]
            Ellipse(q2, x, y, x + d, y + d);
            SelectObject(q, o1);
            SelectObject(q2, o2);
        }
    }
    if (dots) DeleteObject(dots);
    return draws;
}

HallGdi::HallGdi(std::function<gfx::Gfx*()> gfx, gfx::LaunchTodo todo)
    : gfx_(std::move(gfx)), todo_(std::move(todo)), desk_(gfx_, todo_) {
    // seg15:0x9e6 at the launch's step 5 finds DGROUP:0x92b0 and takes the Hall's body and flame
    // square (seg15:0x2a08's branch B early exit) [BYTES seg15:0x2c31-0x2c48].
    desk_.set_visit_pictures([this] {
        desk::LaunchPictures p;
        p.body = body_;
        p.square = square_;
        return p;
    });
}

HallGdi::~HallGdi() = default;

HDC HallGdi::screen() const {
    gfx::Gfx* x = gfx_();
    return x && !x->surface().empty() ? x->dc() : nullptr;
}

void HallGdi::black(int W, int H) {
    if (HDC s = screen()) PatBlt(s, 0, 0, W, H, BLACKNESS);
}

// seg15:0x302e's drawing in state 8 [BYTES seg15:0x3079-0x3519] (verified claim 10, CORRECTED).
std::optional<desk::Rect> HallGdi::landing(int W, int H, int play_h, const std::string& slow, bool render) {
    gfx::Gfx* x = gfx_();
    black(W, H);                     // DGROUP:0x93a6 = 0 [BYTES seg15:0x3079-0x3094]
    if (x) x->palette_save(0, 1);    // seg10:0x6da6(0, 1)
    if (!slow.empty() && x) x->font_text(0, 6, 0x0000FF00, half16(W), half16(play_h), slow);
    if (!render || !x || !scene) return std::nullopt;
    // seg10:0x9526(out, DGROUP:0x92a6, 0x6e, 0 x6, W, H, 0, 0, 0) [BYTES seg15:0x3267-0x3284].
    const desk::Render r = scene(110, W, H);
    scene110_ = x->create_image(W, H);
    desk::draw_scene(*x, scene110_->hdc(), r, todo_);
    if (HDC s = screen()) BitBlt(s, 0, 0, W, H, scene110_->hdc(), 0, 0, SRCCOPY);  // [BYTES seg15:0x34fc-0x3519]
    if (r.slots.empty()) return desk::Rect{};
    return r.slots[0];
}

// seg15:0x1a42 outside state 9 [BYTES seg15:0x1a4f-0x1e7a]: seg15:0x20ca, then the 16 jet frames.
void HallGdi::jet_frames(bool dead, shell::Rng& rng) {
    body_.reset();
    square_.reset();
    flameless_.reset();
    sz_ = 0;
    free_frames();
    gfx::Gfx* g = gfx_();
    if (!g || !ship_look_slot()) return;
    gfx::VisitShipArt look = ship_look_slot()();
    detail_ = look.detail;
    look.dead = false;  // the marks are drawn here (dead_ship_marks)
    const gfx::LaunchShip sh = gfx::ship_20ca(*g, look, todo_);
    body_ = sh.body;
    square_ = sh.square;
    flameless_ = sh.flameless;
    sz_ = sh.ok() ? sh.s : 0;
    if (dead && square_ && flameless_ && sz_ > 0) {
        // seg15:0x20ca calls seg15:0x28b2 inside its thrust-over and thrust-behind paths [BYTES
        // seg15:0x23b3 seg15:0x27c3]; the port draws the marks after gfx::ship_20ca, which on the
        // thrust-over path is after the flame (step 6) instead of before it [DESIGN] (binding
        // request: the call inside gfx/launch_ship.cpp's `dead`).
        dead_ship_marks(square_->hdc(), flameless_->hdc(), gfx::kLaunchK, sz_, rng, todo_);
    }
    if (!square_ || sz_ <= 0) return;
    const std::array<gfx::JetFrame, 16> t = gfx::jet_table(sz_, detail_);
    const int total = gfx::jet_table_height(t);
    if (t[0].w <= 0 || total <= 0) return;
    for (std::size_t i = 0; i < t.size(); ++i) tab_[i] = {t[i].w, t[i].h, t[i].y};
    frames_ = g->create_image(t[0].w, total);
    HDC d = frames_->hdc();
    PatBlt(d, 0, 0, t[0].w, total, BLACKNESS);
    SetStretchBltMode(d, COLORONCOLOR);
    const int ks = lo16(gfx::kLaunchK * sz_);
    for (const Frame& f : tab_) StretchBlt(d, 0, f.y, f.w, f.h, square_->hdc(), 0, 0, ks, ks, SRCCOPY);
    frames_mask_ = g->mask(*frames_);
}

void HallGdi::free_frames() {
    frames_.reset();
    frames_mask_.reset();
    tab_ = {};
}

// seg15:0x3caa(first): the same routine as the visit's (src/hud/visit_win32.cpp VisitGdi::incoming,
// s6-scene-verify.md claims 61-68), over the scene DC of scene 110 [BYTES seg15:0x3cc6-0x4540].
int HallGdi::incoming(int first, int d92d6, int d92d8, int d92de) {
    gfx::Gfx* g = gfx_();
    HDC s = screen();
    Jet& j = jet_;
    if (!g || !s || !frames_ || !frames_mask_ || tab_[0].w <= 0) return 1;
    const int W = g->logical_width();
    const Frame& f0 = tab_[0];
    if (first != 0) {
        d92d6_ = d92d6;
        d92d8_ = d92d8;
        j.y10 = lo16(10 * d92d8);
        j.phase = 0;
        j.clip = {0, 0, W, d92de};
        work_ = g->create_image(f0.w, f0.h + 15);
        j.step = idiv16(lo16(d92d8 - d92d6), 16);
        j.speed = 150;
        j.idx = 0;
        j.level = 100;
        j.started = true;
    }
    if (!j.started || !work_) return 1;
    if (j.step == 0) j.step = 1;
    HDC w = work_->hdc();
    const int x0 = half16(W) - half16(f0.w);
    const auto compose = [&](int top) {
        const RECT r{x0, top, x0 + f0.w, top + f0.h + 15};
        PatBlt(w, 0, 0, r.right - r.left, r.bottom - r.top, BLACKNESS);
        RECT c{};
        if (scene110_ && IntersectRect(&c, &r, &j.clip))
            BitBlt(w, c.left - r.left, c.top - r.top, c.right - c.left, c.bottom - c.top, scene110_->hdc(), c.left,
                   c.top - j.clip.top, SRCCOPY);
        return r;
    };
    const auto shadow = [&](int h) {
        const Frame& fi = tab_[static_cast<std::size_t>(j.idx)];
        const int fx = j.idx < 10 ? 25 - 2 * j.idx : 15 - j.idx;
        const int fy = j.idx < 10 ? 4 * (15 - j.idx) : 30 - j.idx;
        gfx::shadow_blit(w, half16(f0.w) - half16(fi.w) + fx, fy, fi.w, h, frames_mask_->hdc(), 0, fi.y);
    };
    const auto frame = [&](const Frame& f, int y) {
        BitBlt(w, half16(f0.w) - half16(f.w), y, f.w, f.h, frames_mask_->hdc(), 0, f.y, SRCAND);
        BitBlt(w, half16(f0.w) - half16(f.w), y, f.w, f.h, frames_->hdc(), 0, f.y, SRCPAINT);
    };
    if (j.phase == 1) {
        j.speed = std::max(j.speed - 5, 50);
        const RECT r = compose(idiv16(lo16(j.y10), 10));
        shadow(tab_[static_cast<std::size_t>(j.idx)].h);
        frame(tab_[15], 2);
        BitBlt(s, r.left, r.top, r.right - r.left, r.bottom - r.top, w, 0, 0, SRCCOPY);
        j.y10 = lo16(j.y10 + idiv16(lo16(2 * j.speed), 20));
        return j.y10 > lo16(10 * (d92d6_ + 44)) ? 1 : 0;
    }
    const int old = j.idx;
    j.idx = std::clamp<int>(idiv16(lo16(idiv16(lo16(j.y10), -10) + d92d8_ - 55), lo16(j.step)), 0, 15);
    const int y = idiv16(lo16(j.y10), 10);
    if (first != 0) j.prevy = y;
    if (old < j.idx && j.phase == 0) {
        int lv0 = j.level;
        j.level = idiv16(lo16(-100 * j.idx), 30) + 100;
        if (lv0 == 0) lv0 = 1;
        const int p = idiv16(lo16(100 * j.level), lo16(lv0));
        g->palette_fade(3, p);
        g->palette_fade(2, idiv16(lo16(p + 200), 3));
    }
    j.speed = std::max(150 - 4 * j.idx, 9);
    const RECT r = compose(y);
    {
        const Frame& fi = tab_[static_cast<std::size_t>(j.idx)];
        const int fy = j.idx < 10 ? 4 * (15 - j.idx) : 30 - j.idx;
        shadow(std::min(f0.h - (j.prevy - y) - fy + 14, fi.h));
    }
    frame(tab_[static_cast<std::size_t>(j.idx)], 0);
    BitBlt(s, r.left, r.top, r.right - r.left, r.bottom - r.top, w, 0, 0, SRCCOPY);
    j.prevy = y;
    j.y10 = lo16(j.y10 + idiv16(lo16(2 * j.speed), -3));
    if (j.y10 < lo16(10 * d92d6_)) {
        j.y10 = lo16(j.y10 - 20);
        j.speed = 200;
        j.phase = 1;
    }
    return 0;
}

// seg15:0x1328(1) in state 8 and seg15:0x1eac [BYTES seg15:0x1eba-0x2081]: the jet's frames freed,
// then frame 0 (with the flame) and frame 1 (without) stretched to s4 x s4 each, with their mask.
void HallGdi::two_frames() {
    jet_.started = false;
    work_.reset();
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

// seg15:0x760's render branch [BYTES seg15:0x8a2-0x9d8]: scene 0x73 into the scene DC (the desk
// canvas's), its slot 0 and word 5.
Screen::HallScene HallGdi::hall_scene(int W, int H) {
    scene110_.reset();
    HallScene hs;
    if (!scene) return hs;
    const desk::Render r = scene(115, W, H);  // [BYTES seg15:0x903]
    desk_.scene(r);
    if (!r.slots.empty()) hs.slot0 = r.slots[0];
    if (r.slots.size() > 1) hs.w5 = r.slots[1].t;  // out word 5 (`es:[bx + 0xa]`)
    return hs;
}

// [BYTES seg15:0x796-0x860]: frame 1 at (x, y) through the mask (0x008800C6) and the image
// (0x00EE0086).
void HallGdi::ship_frame(int x, int y, int s4) {
    HDC s = screen();
    if (!s || !frames_ || !frames_mask_) return;
    BitBlt(s, x, y, s4, s4, frames_mask_->hdc(), 0, tab_[1].y, SRCAND);
    BitBlt(s, x, y, s4, s4, frames_->hdc(), 0, tab_[1].y, SRCPAINT);
}

// seg15:0x374c's drawing in state 8 [BYTES seg15:0x39b6-0x3b92 seg15:0x3a4d-0x3a76
// seg15:0x3aa4-0x3b12]: the work DC seg15:0x3bfa(s4, s4 + 40) gets the scene DC's part under the
// ship, the shadow at every height and the frame, then goes to the screen. The port puts the scene
// DC's part on the screen and copies it back into the work DC, the same pixels [DESIGN].
void HallGdi::descent_frame(int W, int y, int s4, int flag, int hgt) {
    HDC s = screen();
    gfx::Gfx* g = gfx_();
    if (!s || !g || s4 <= 0) return;
    if (!work_ || work_->width() != s4 || work_->height() != s4 + 40) work_ = g->create_image(s4, s4 + 40);
    HDC w = work_->hdc();
    const int left = half16(W) - half16(s4);
    desk_.scene_to_screen(left, y, s4, s4 + 40);
    BitBlt(w, 0, 0, s4, s4 + 40, s, left, y, SRCCOPY);
    if (frames_ && frames_mask_) {
        gfx::shadow_blit(w, 0, half16(hgt) + 9, s4, s4, frames_mask_->hdc(), 0, tab_[1].y);
        const int fy = tab_[flag != 0 ? 1 : 0].y;
        BitBlt(w, 0, 5, s4, s4, frames_mask_->hdc(), 0, fy, SRCAND);
        BitBlt(w, 0, 5, s4, s4, frames_->hdc(), 0, fy, SRCPAINT);
    }
    BitBlt(s, left, y, s4, s4 + 40, w, 0, 0, SRCCOPY);
}

}  // namespace is::hall
#endif
