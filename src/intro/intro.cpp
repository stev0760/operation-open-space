#include "intro/intro.h"

#include <cstring>

namespace is::intro {

namespace {

constexpr Colour kBlack = 0x00000000;
constexpr Colour kWhite = 0x00FFFFFF;
// Logo step 2's pen colours by rand() % 4: blue, cyan, white, teal [BYTES seg15:0x60b9
// seg15:0x60de].
constexpr Colour kSweepPens[4] = {0x00FF0000, 0x00FFFF00, 0x00FFFFFF, 0x00808000};
// DKGRAY_BRUSH (stock 3, RGB(64, 64, 64)) for the MERGECOPY sweep [BYTES seg15:0x6212]. A
// 256-colour display dithers that grey; the port takes the palette's nearest colour
// (PALETTERGB) instead, a dark grey, where the nearest static colour would be black [DESIGN].
constexpr Colour kDkGrayBrush = 0x02404040;
// The title's colours (shell.md "Title (phase 2)").
constexpr Colour kSilver = 0x02C0C0C0;     // PALETTERGB(0xC0, 0xC0, 0xC0)
constexpr Colour kLilac = 0x02E080B4;      // PALETTERGB(0xB4, 0x80, 0xE0)
constexpr Colour kTitleBlue = 0x02FFA500;  // PALETTERGB(0x00, 0xA5, 0xFF), overwritten
constexpr Colour kShadow = 0x02C00060;     // PALETTERGB(0x60, 0x00, 0xC0)
constexpr Colour kMain = 0x02FF60A0;       // PALETTERGB(0xA0, 0x60, 0xFF)
// The bolt's pens: RGB(0xC3, 0xFF, 0xFF) (type 0xe), the flash RGB(0x7F, 0xFF, 0xFF)
// [BYTES seg11:0xa8a3 seg11:0xaa02].
constexpr Colour kBoltPen = 0x00FFFFC3;
constexpr Colour kFlashPen = 0x00FFFF7F;

const FontSpec kPresentsFont{38, 0, 0, true, "Times New Roman"};  // seg15:0x6479
const FontSpec kOperationFont{24, 11, 600, false, "Arial"};       // seg15:0x6d20
const FontSpec kNameFont{35, 19, 0, false, "Arial"};              // seg15:0x6e02
const FontSpec kLineFont{22, 0, 0, false, "Arial"};               // seg15:0x6eb9
const FontSpec kSmallFont{16, 0, 500, false, "Arial"};            // seg15:0x71d3
const FontSpec kEscFont{30, 0, 800, true, "Arial"};               // seg15:0x72f7
const FontSpec kCreditBig{28, 20, 0, false, "Arial"};             // seg15:0x8213
const FontSpec kCreditSmall{22, 13, 0, false, "Arial"};           // seg15:0x823a
const FontSpec kPageFont{18, 10, 0, false, "Arial"};              // seg15:0x69cc

// The logo cell's source x in the strip: 6 + 46 cell [BYTES seg15:0x5f2e].
int cell_x(int cell) { return 6 + 46 * cell; }

}  // namespace

Intro::Intro(Canvas& canvas, Host& host, shell::Rng& rng) : canvas_(canvas), host_(host), rng_(rng) {}

void Intro::todo_once(const std::string& what, const char* address) {
    if (logged_.insert(what).second) host_.todo(what, address);
}

// ---------------------------------------------------------------- the state-0 entry

void Intro::enter(Tick now) {
    now_ = now;
    area_ = host_.area();
    // 1: the black play area [BYTES seg15:0x57a7]. seg10:0x4b2e rebuilds INSPACE's brush and pen
    // tables, which no intro phase reaches (shell.md "Phase 2 entry calls"); the port has none.
    canvas_.black(0, 0, area_.w, area_.h);
    host_.music(-1);  // seg10:0x7480(-1, 1) [BYTES seg15:0x57b5]
    // 2: the DEMO branch needs DGROUP:0x90f8 (the DEMO command line, stage 3). DGROUP:0x1a is
    // cleared by GameState; DGROUP:0x9516 = 0 matters only to the demo start.
    registration_check();  // seg15:0x5803
    // 3 [BYTES seg15:0x5809 seg15:0x5821 seg15:0x582e seg15:0x5834]
    phase_ = 0;
    deadline_ = now + 65000;
    step_ = 0;
    speed_ = 290;
    spin_ = 5000;
    host_.show_window();     // 4: seg15:0x5840
    canvas_.load_sd_logo();  // 5: seg15:0x5914
    // The two 8 x 8 pattern brushes (DGROUP:0x5ea8, DGROUP:0x5eaa) serve no intro phase
    // (shell.md "Pattern brushes"); realising the palette has no port equivalent (ADR 0007
    // decision 5). The strip: seg10:0x552(5, 1) and seg10:0x39a8(0) when Size is not 1; the
    // port builds the unscaled strip whenever it has none, which is what both paths leave.
    canvas_.build_strip();
    frame(now, true);  // 6: seg15:0x58c3
    todo_once("sound start (ISPACE ordinal 7 command 0)", "seg10:0x73e2");
    sound(0x6e);                // 7 [BYTES seg15:0x58e2]
    host_.music(-5);            // seg10:0x7480(-5, 1) [BYTES seg15:0x58ec]
    host_.calibrate_start(now);  // seg15:0x58fb
    canvas_.release_dc();
}

// The registration check seg10:0x885e(0) is stage 3 (it needs ISPACE ordinal 24 and the INI
// keys of shell.md "Registration check"). The port takes the unregistered Limited edition the
// oracle ran: DGROUP:0x920c = 0, DGROUP:0x922a = 0, DGROUP:0x11aa = 0, and the day from the local
// date with V = P = 0 ("Trial day"): DGROUP:0x9224 = dayval - 254. TODO stage 3.
void Intro::registration_check() {
    edition_ = 0;
    invalid_ = 0;
    plus_ = 0;
    const LocalDay t = host_.local_day();
    day_ = trial_day(day_value(t.tm_year, t.tm_yday), 0, 0);
}

// ---------------------------------------------------------------- the frame seg15:0x5962

void Intro::frame(Tick now, bool redraw) {
    now_ = now;
    area_ = host_.area();
    if (phase_ == 7) return;  // seg15:0x59d5: idle while the demo set-up runs
    phase_work(redraw);
    // [BYTES seg15:0x59df]: the frame returns while DGROUP:0x6e28 >= DGROUP:0x1e50 (unsigned).
    if (now_ > deadline_) transition();
    canvas_.release_dc();
}

void Intro::phase_work(bool redraw) {
    switch (phase_) {  // seg15:0x59c2
    case 0:
        attract_ = true;  // DGROUP:0x95bc = 1
        logo(redraw);
        break;
    case 2: title(); break;
    case 3: credits(); break;
    case 4:
        if (redraw) text_page();
        break;
    case 5:
        if (redraw && deadline_ != now_) keys();
        break;
    default: break;  // 1: nothing; 6 (the desk scene) is not entered by the port
    }
}

void Intro::transition() {
    const bool edition01 = edition_ == 0 || edition_ == 1;
    if (phase_ == 4 && page_ < 1) {  // seg15:0x59e8: the second page
        ++page_;
        deadline_ = now_ + (edition01 ? 27000 : 17000);
        text_page();
        return;
    }
    sound(0x6e);                                  // seg15:0x5a23
    canvas_.black(0, 0, area_.w, area_.h);        // seg15:0x5a3d
    ++phase_;                                     // seg15:0x5a42
    // A new phase 2 under DEMO with QuickIntro (DGROUP:0x90f8, DGROUP:0x8f70) also takes the
    // demo-start path [BYTES seg15:0x5a46]; the port has no DEMO command line yet.
    if (phase_ > 5) {
        // The demo start seg15:0x5a64 (DGROUP:0x95bc = 0, DGROUP:0x95b8 = 7, the demo set-up)
        // is stage 3. The port restarts state 0 at the logo instead, as a demo that ends with
        // ESC does (shell.md "Self-playing demo") [DESIGN].
        attract_ = false;
        phase_ = 7;
        todo_once("demo start after the intro; state 0 restarts at the logo", "seg15:0x5a64");
        host_.demo_start();
        return;
    }
    switch (phase_) {  // seg15:0x5cfb
    case 1:
        // seg10:0x7480(0, 1) unless DGROUP:0x8f70 and DGROUP:0x90f8 are both set (DEMO).
        host_.music(0);
        deadline_ = now_ + 1000;
        break;
    case 2: title_enter(); break;
    case 3: credits_enter(); break;
    case 4:
        // seg10:0x7d58 (ordinal 8) only positions sound.
        deadline_ = now_ + (edition01 ? 25000 : 18000);
        page_ = 0;
        text_page();
        break;
    case 5:
        deadline_ = now_ + 11000;
        keys();
        break;
    default: break;
    }
}

void Intro::title_enter() {
    // seg10:0x4b2e and seg6:0x7266 touch nothing the title draws (shell.md "Phase 2 entry
    // calls") [BYTES seg15:0x5c0e seg15:0x5c13].
    deadline_ = now_ + 65000;
    step_ = 0;
    scene_off_ = true;           // DGROUP:0x94c4 = 1
    canvas_.palette_scheme(8, 1);  // seg15:0x5c35
    d_ = title_offset(area_.h);
    c8_ = 0;
    ca_ = 0;
    rows_ = 0;
    std::memset(row_map_, 0, sizeof row_map_);
}

// ---------------------------------------------------------------- phase 0: the logo

void Intro::logo_cell(int cell) {
    last_cell_ = cell;
    canvas_.blit(Image::kStrip, area_.w / 2 - 88, area_.h / 2 - 60, 40, 40, cell_x(cell), 6);
}

void Intro::logo(bool redraw) {
    // seg15:0x5e81: a redraw after step 0 blits the cell of the uninitialised local [bp-0x10];
    // the port blits the cell it drew last [DESIGN].
    if (redraw && step_ != 0) logo_cell(last_cell_);
    switch (step_) {
    case 0: {
        const int f = spin_ < 9999 ? spin_ / 1000 : 0;  // seg15:0x5e9b
        if (host_.calibrated() && !host_.calibrating() && speed_ != 0 && speed_ < 160 && f == 5) {
            spin_ = 0;  // seg15:0x5ee7
            speed_ = 0;
            sound(0x23);
            ++step_;
        }
        logo_cell(f);
        spin_ += speed_;
        if (spin_ > 13999) {  // seg15:0x5f36
            spin_ = 0;
            if (!host_.calibrating()) {
                speed_ -= 18;
                if (speed_ < 1) speed_ = 1;
            }
        }
        // The Calibrate measurement (seg15:0x5f68); its end sets DGROUP:0x95d0 = 240
        // [BYTES seg15:0x6009].
        if (host_.calibrating() && host_.calibrate_step(now_)) speed_ = 240;
        break;
    }
    case 1:  // [BYTES seg15:0x6065-0x60b2]
        logo_cell(spin_ / 1000 + 10);
        spin_ += 45;
        if (spin_ > 3939) {
            sound(0x68);
            spin_ = 620;
            ++step_;
        }
        break;
    case 2: sweep(); break;
    case 3:  // [BYTES seg15:0x63ad-0x6429]
        logo_cell(spin_ / 1000 + 10);
        spin_ -= 45;
        if (spin_ < 61) {
            logo_cell(5);
            ++step_;
            deadline_ = now_ + 3000;
        }
        break;
    case 4: presents(); break;
    default: break;
    }
}

// Logo step 2: the lines, the column reveal and the final yellow logo [BYTES seg15:0x60b9-0x637e].
void Intro::sweep() {
    const int w2 = area_.w / 2, h2 = area_.h / 2;
    const int v = spin_;
    const Colour pen = kSweepPens[rng_.rand() % 4];
    const int x1 = w2 + v / 5 - 38;
    const int y1 = rng_.rand() % 30 + h2 - 55;
    canvas_.line(pen, 2, w2 - 48, h2 - 40, x1, y1);
    const int y2 = rng_.rand() % 30 + h2 - 55;
    canvas_.line(kBlack, 2, w2 - 48, h2 - 40, x1, y2);
    // seg10:0x7d58(W/2 + v/2 - 38, DGROUP:0x90ec / 2) only positions sound.
    canvas_.merge(Image::kSdLogo, v / 5 + w2 - 38, h2 - 56, 2, 32, v / 5, 0, kDkGrayBrush);
    spin_ -= 4;
    if (spin_ >= 0) return;
    canvas_.black(w2 - 48, h2 - 56, 10, 32);
    for (int i = 0; i < 0x276; i += 2) {
        if (i == 100) sound(0x6e);
        const int q = i / 5;
        const Colour brush = static_cast<Colour>((128 + q) & 0xff) |
                             (static_cast<Colour>((128 + q) & 0xff) << 8) |
                             (static_cast<Colour>((127 - q) & 0xff) << 16);
        canvas_.merge(Image::kSdLogo, w2 - 38, h2 - 56, 130, 32, 0, 0, brush);
    }
    ++step_;
    spin_ = 3940;
    sound(0x23);
}

// Logo step 4: "presents.. " [BYTES seg15:0x643e-0x6500].
void Intro::presents() {
    sound(0x6e);
    const FontId f = canvas_.create_font(kPresentsFont);
    const FontId old = canvas_.select_font(f);
    const Colour bk = canvas_.bk_colour(kBlack);
    canvas_.bk_mode(1);
    canvas_.text_align(6);
    const Colour tc = canvas_.text_colour(0x0000FFFF);
    canvas_.text_out(area_.w / 2, area_.h / 2 + 17, load_limit(host_.lang(2118), 0x28));
    canvas_.bk_colour(bk);
    canvas_.text_colour(tc);
    canvas_.select_font(old);
    canvas_.delete_font(f);
}

// ---------------------------------------------------------------- phase 2: the title

// seg15:0x7970: white at (x - 1, y - 1), the shadow at (x + 1, y + 1), the main colour at
// (x, y); DGROUP:0x11aa = 0 at 8 bpp [BYTES seg15:0x7990-0x7a51].
void Intro::emboss(int x, int y, const std::string& text) {
    canvas_.text_colour(kWhite);
    canvas_.text_out(x - 1, y - 1, text);
    Colour shadow = kShadow, main = kMain;
    if (plus_ != 0) {
        shadow = kDepth < 8 ? 0x00000080 : 0x02195374;
        main = kDepth < 8 ? 0x0000FFFF : 0x0256BCD8;
    } else if (kDepth < 8) {
        main = 0x00FF00FF;
    }
    canvas_.text_colour(shadow);
    canvas_.text_out(x + 1, y + 1, text);
    canvas_.text_colour(main);
    canvas_.text_out(x, y, text);
}

void Intro::title_first_frame() {
    const TitleLayout at = title_layout(area_.w, area_.h, d_, c_);
    canvas_.palette_ramp(5);  // seg15:0x6cfb
    FontId f = canvas_.create_font(kOperationFont);
    FontId old = canvas_.select_font(f);
    const Colour bk = canvas_.bk_colour(kBlack);
    canvas_.bk_mode(1);
    canvas_.text_align(6);
    const Colour tc = canvas_.text_colour(kDepth < 8 ? 0x00FFFF00 : kTitleBlue);
    emboss(at.operation.x, at.operation.y, load_limit(host_.lang(2121), 0x5a));
    canvas_.select_font(old);
    canvas_.delete_font(f);

    f = canvas_.create_font(kNameFont);
    old = canvas_.select_font(f);
    emboss(at.name.x, at.name.y, title_name_line(host_.lang(edition_ == 0 ? 2338 : 2337)));
    canvas_.select_font(old);
    canvas_.delete_font(f);

    f = canvas_.create_font(kLineFont);
    old = canvas_.select_font(f);
    std::string edition;
    if (plus_ != 0) edition = host_.lang(2340);
    else if (edition_ != 3) edition = host_.lang(2339);
    const std::string version = version_string(host_.exe_string(167), host_.exe_string(233));
    canvas_.text_colour(kSilver);
    emboss(at.edition.x, at.edition.y, edition_line(edition, version, host_.exe_string(607)));
    registration_check();  // seg15:0x6f9d
    canvas_.text_colour(kSilver);
    if (edition_ == -1) {
        // "CD: " or 2124 "Reg User: " with seg10:0x946a: registered copies are stage 3.
        todo_once("title line of a registered copy", "seg15:0x6fda");
    }
    const TrialLine trial =
        trial_line(edition_, invalid_, day_, host_.lang(2341), host_.lang(2342), host_.lang(2343));
    if (!trial.none) emboss(at.trial.x, at.trial.y, trial.text);
    if (trial.order_line) {  // [BYTES seg15:0x70fb-0x7145]
        canvas_.text_colour(kBlack);
        canvas_.bk_colour(kLilac);
        canvas_.bk_mode(2);
        canvas_.text_out(at.order.x, at.order.y, host_.lang(invalid_ == 0 ? 2310 : 2344));
        canvas_.bk_colour(kBlack);
        canvas_.bk_mode(1);
    }
    canvas_.select_font(old);
    canvas_.delete_font(f);

    f = canvas_.create_font(kSmallFont);
    old = canvas_.select_font(f);
    // The hint DGROUP:0x129d needs DGROUP:0x90ee != 8: never on the port (ADR 0007 decision 5).
    canvas_.text_colour(kLilac);
    canvas_.text_out(at.copyright.x, at.copyright.y, kCopyright);
    canvas_.select_font(old);
    canvas_.delete_font(f);

    f = canvas_.create_font(kEscFont);
    old = canvas_.select_font(f);
    canvas_.text_colour(kLilac);
    emboss(at.esc.x, at.esc.y, host_.lang(2542));
    canvas_.bk_colour(bk);
    canvas_.text_colour(tc);
    canvas_.select_font(old);
    canvas_.delete_font(f);

    // The picture [BYTES seg15:0x7394-0x758c]: graphic 300's first copy, rows 0-104 doubled.
    canvas_.load_title_picture();
    canvas_.double_copy(Image::kTitle, at.picture.x, at.picture.y, 0, 0, 138, 105);
    canvas_.free_image(Image::kTitle);
    std::memset(row_map_, 0, sizeof row_map_);
    // At 8 bpp and above the second copy is ordinal 12 (seg15:0x757c -> seg10:0x73a6).
    canvas_.make_title_reveal();
    todo_once("ordinal 12's smoothed 2x picture ([OPEN]); the title reveals a plain 2x copy",
              "seg10:0x73a6");
}

void Intro::title() {
    bool reset = false;
    switch (step_) {
    case 0: {
        if (rows_ == 0) title_first_frame();
        timer2_ = 0;  // seg15:0x75ad
        c6_ = area_.w / 2 - 400;
        which_ = 0;
        int r = 0;
        for (;;) {  // seg15:0x75cb: retried while bit r of the row map is set
            r = rng_.rand() % 105;
            if ((row_map_[r >> 3] & (1u << (r & 7))) == 0) break;
        }
        row_map_[r >> 3] = static_cast<std::uint8_t>(row_map_[r >> 3] | (1u << (r & 7)));
        ++rows_;
        const TitleLayout at = title_layout(area_.w, area_.h, d_, c_);
        canvas_.blit(Image::kTitleReveal, at.picture.x, at.picture.y + 2 * r, 276, 2, 0, 2 * r);
        if (rows_ < 105) break;
        canvas_.free_image(Image::kTitleReveal);  // seg15:0x7652
        std::memset(row_map_, 0, sizeof row_map_);
        ++step_;
        rows_ = 0;
        reset = true;
        break;
    }
    case 1:  // seg15:0x76c3
        step_ = 2;
        c8_ = 0;
        c_ = 0;
        break;
    case 2:
        ++step_;
        reset = true;
        break;
    case 3:  // seg15:0x76d2: 115 frames; seg10:0x7d58 only positions sound
        c6_ += 5;
        c8_ += 2;
        ca_ += 2;
        if (ca_ < 0xe6) break;
        ++step_;
        reset = true;
        break;
    case 4:  // seg15:0x7729: 20 frames
        c6_ += 5;
        c8_ += 4;
        c_ += 2;
        if (c_ < 0x28) break;
        timer2_ = now_ + 45000;
        deadline_ = now_ + 51500;
        step_ = 5;
        reset = true;
        break;
    default: break;  // step 5 has no case
    }
    if (reset) {  // seg15:0x76ba
        c8_ = 0;
        ca_ = 0;
    }
    // seg15:0x7792: the scripted scene (seg6 objects and effects) is not ported; the title
    // stays static until the phase deadline [DESIGN].
    if (timer2_ != 0 && timer2_ < now_) {
        todo_once("scripted title scene", "seg15:0x7792");
        timer2_ = 0;
    }
}

// ---------------------------------------------------------------- phase 3: the credits

void Intro::credits_enter() {  // seg15:0x7b34
    deadline_ = now_ + 90000;
    which_ = 0;
    canvas_.palette_scheme(8, 0);
    canvas_.palette_animate();
    // seg10:0x58ce(0) loads #4000 340 with mode 3 when Enhance is set, else 0; at 8 bpp mode 3
    // is mode 0 (ispace.md ordinal 1).
    canvas_.build_credit_cells(0);
    sound(0x13);
    speed_ = 50;
    entry_ = 0;
    timer2_ = 0;
    load_entry();
}

void Intro::load_entry() {  // seg15:0x811a
    heading_ = load_limit(host_.lang(1000 + entry_), 0x3c);
    name_ = load_limit(host_.lang(980 + entry_), 0x3c);
    which_ = 0;
    timer2_ = 0;
    deadline_ = now_ + 90000;
}

void Intro::credits_logo(int s) {
    canvas_.stretch(Image::kCredits, area_.w / 2 - s / 2, area_.h / 2 - s / 2 - 20, s, s,
                    (spin_ / 10) * 42 + 4, 4, 38, 38);
}

void Intro::credits() {  // seg15:0x7cda
    const int calibrate = host_.calibrate_value();
    const int step = calibrate < 800 ? 6 : 3;
    spin_ += step;
    if (spin_ > 0x58) spin_ = 0;
    speed_ += step;
    if (speed_ > 300) speed_ = 300;
    const int s = speed_ * 38 / 200;
    credits_logo(s);
    if (speed_ != 300) return;
    if (next_entry_ != 0) {
        if (next_entry_ < now_) {
            load_entry();
            next_entry_ = 0;
        }
        return;
    }
    if (timer2_ == 0) {
        if (type_line(which_, typed_) == 0) {
            typed_ += calibrate < 0x385 || host_.mix_channels() < 2 ? 4 : 2;
            // an odd count skips the sound [BYTES seg15:0x7e55 seg15:0x7e5a]; at DGROUP:0x3b98
            if ((typed_ & 1) == 0) sound(0x14);
        } else {
            if (which_ == 0) {
                finish_bolt();
                type_line(0, -1);
                ++which_;
            } else {
                timer2_ = now_ + 3000;
                finish_bolt();
                type_line(which_, -1);
            }
            typed_ = 0;
        }
    } else if (timer2_ < now_) {
        erase(s);
    }
}

// The erase of an entry [BYTES seg15:0x7e7b-0x809b].
void Intro::erase(int s) {
    const int w2 = area_.w / 2, h2 = area_.h / 2, y1 = h2 - 20;
    const int k = host_.mix_channels() == 1 ? 1 : 4;
    for (int i = 0; i < k; ++i) {
        const int x = rng_.rand() % 300 + w2 - 150;
        aim_bolt(w2, y1, x, h2 - 150);
        sound(0x22);
        host_.present_wait(40);
    }
    finish_bolt();
    canvas_.black(0, h2 - 150, area_.w, 40);
    credits_logo(s);
    host_.present_wait(270);
    for (int i = 0; i < k; ++i) {
        const int x = rng_.rand() % 300 + w2 - 150;
        aim_bolt(w2, y1, x, h2 + 100);
        sound(0x22);
        host_.present_wait(40);
    }
    finish_bolt();
    canvas_.black(0, h2 + 100, area_.w, 40);
    now_ = host_.now();  // DGROUP:0x1e50 = GetCurrentTime
    ++entry_;
    if (entry_ < 6) {
        next_entry_ = now_ + 1000;
    } else {
        deadline_ = now_ + 4000;
        timer2_ = now_ + 9000;
    }
}

// seg15:0x8174(which, c): 0 when it typed (or measured), 1 when the line is complete
// [BYTES seg15:0x8183-0x8485].
int Intro::type_line(int which, int c) {
    if (entry_ >= 6) return 1;
    const int raw = c;
    int half = c / 2;
    const std::string& text = which == 0 ? heading_ : name_;
    const int yoff = which == 0 ? -150 : 100;
    const int len = static_cast<int>(text.size());
    if (raw == -1) half = len;
    else if (len < half) return 1;
    const FontId big = canvas_.create_font(kCreditBig);
    const FontId small = canvas_.create_font(kCreditSmall);
    // a-z are drawn uppercased in the small font (small capitals); the compare is signed
    const auto glyph = [&](char ch, std::string* out) {
        const auto sc = static_cast<signed char>(ch);
        if (sc < '{' && sc > '`') {
            *out = std::string(1, static_cast<char>(sc - 0x20));
            return small;
        }
        *out = std::string(1, ch);
        return big;
    };
    const int w2 = area_.w / 2, h2 = area_.h / 2;
    if (half == 0) {  // the width, on the first call of a line [BYTES seg15:0x8264-0x82fc]
        canvas_.move_to(-2000, 0);
        canvas_.text_align(1);
        line_width_ = 0;
        for (char ch : text) {
            std::string g;
            const FontId old = canvas_.select_font(glyph(ch, &g));
            canvas_.text_out(0, 0, g);
            canvas_.select_font(old);
        }
        line_width_ = canvas_.current_position().x + 2000;
    }
    const int aim = len != 0 ? line_width_ * half / len : 0;
    if (raw != -1) aim_bolt(w2, h2 - 20, w2 - line_width_ / 2 + aim, h2 + yoff);
    const Colour bk = canvas_.bk_colour(kBlack);
    canvas_.bk_mode(1);
    canvas_.move_to(w2 - line_width_ / 2, h2 + yoff);
    const Colour tc = canvas_.text_colour(kDepth < 8 ? 0x00FFFF00 : kTitleBlue);
    for (int i = 0; i < half && i < len; ++i) {
        std::string g;
        const FontId old = canvas_.select_font(glyph(text[static_cast<std::size_t>(i)], &g));
        emboss_char(g);
        canvas_.select_font(old);
    }
    canvas_.bk_colour(bk);
    canvas_.text_colour(tc);
    canvas_.delete_font(big);
    canvas_.delete_font(small);
    return 0;
}

void Intro::emboss_char(const std::string& ch) {  // seg15:0x7a62
    const Point p = canvas_.current_position();
    canvas_.text_colour(kWhite);
    canvas_.text_out(p.x - 1, p.y - 1, ch);
    canvas_.text_colour(kShadow);
    canvas_.text_out(p.x + 1, p.y + 1, ch);
    canvas_.text_colour(kDepth < 8 ? 0x00FF00FF : kMain);
    canvas_.text_out(p.x, p.y, ch);
    canvas_.move_to(p.x + canvas_.text_width(ch), p.y);
}

void Intro::aim_bolt(int x1, int y1, int x2, int y2) {  // seg15:0x80b0
    finish_bolt();
    bolt_end_ = {x2, y2};
    draw_bolt(x1, y1, x2, y2);
}

void Intro::finish_bolt() {  // seg15:0x80da
    if (pending_ == 0) return;
    if (host_.calibrate_value() < 0x578) {
        bolt_pass();
        pending_ = 0;
    } else {
        while (pending_ != 0) bolt_pass();
    }
}

void Intro::clipped_line(Colour pen, int width, Point a, Point b) {
    if (line_visible(a, b, area_.w, area_.play_h)) canvas_.line(pen, width, a.x, a.y, b.x, b.y);
}

void Intro::bolt_lines(Colour main, Colour branch, bool main_too) {
    if (main_too) {
        const int n = pending_ < 0 ? -pending_ : pending_;
        for (int a = 1; a < n && a < static_cast<int>(bolt_.main.size()); ++a)
            clipped_line(main, 2, bolt_.main[static_cast<std::size_t>(a - 1)],
                         bolt_.main[static_cast<std::size_t>(a)]);
    }
    const int width = branch == kBlack && !main_too ? 0 : 1;
    for (const auto& br : bolt_.branches)
        for (std::size_t k = 1; k < br.size(); ++k) clipped_line(branch, width, br[k - 1], br[k]);
    for (const auto& sb : bolt_.subs)
        for (std::size_t m = 1; m < sb.size(); ++m) clipped_line(branch, width, sb[m - 1], sb[m]);
}

// seg11:0xa4a6(x1, y1, x2, y2, 0xe) [BYTES seg11:0xa4bb seg11:0xa8a3-0xa9de].
void Intro::draw_bolt(int x1, int y1, int x2, int y2) {
    rng_.srand(static_cast<std::uint16_t>(host_.now() & 0xffff));  // srand(GetCurrentTime())
    Bolt b;
    if (!make_bolt(rng_, x1, y1, x2, y2, &b)) return;
    bolt_ = std::move(b);
    const int n = static_cast<int>(bolt_.main.size()) - 1;
    for (int a = 1; a <= n; ++a)
        clipped_line(kBoltPen, 2, bolt_.main[static_cast<std::size_t>(a - 1)],
                     bolt_.main[static_cast<std::size_t>(a)]);
    for (const auto& br : bolt_.branches)
        for (std::size_t k = 1; k < br.size(); ++k) clipped_line(kBoltPen, 1, br[k - 1], br[k]);
    for (const auto& sb : bolt_.subs)
        for (std::size_t m = 1; m < sb.size(); ++m) clipped_line(kBoltPen, 1, sb[m - 1], sb[m]);
    pending_ = n + 1;  // DGROUP:0x1016
}

// seg11:0xa9ea(0) [BYTES seg11:0xa9fd seg11:0xaa02 seg11:0xaa65 seg11:0xaaf9 seg11:0xabb0
// seg11:0xabbf seg11:0xac61 seg11:0xac6e seg11:0xac78].
void Intro::bolt_pass() {
    canvas_.bk_mode(2);
    const Colour c = pending_ < 1 ? kFlashPen : kBlack;
    bolt_lines(c, c, true);
    if (pending_ < 1 || flashed_ != 0) {
        if (flashed_ != 0) {
            pending_ = 0;
            flashed_ = 0;
            return;
        }
        flashed_ = 1;
    } else {
        bolt_lines(kBlack, kBlack, false);  // stock BLACK_PEN over the branches
    }
    pending_ = -pending_;
}

// ---------------------------------------------------------------- phases 4 and 5

void Intro::text_page() {  // seg15:0x6890
    const int w2 = area_.w / 2, h2 = area_.h / 2;
    const std::string version = version_string(host_.exe_string(167), host_.exe_string(233));
    const std::string name =
        load_limit(host_.lang(edition_ == 0 ? 2338 : 2337), 0x2d) + " " + version;
    canvas_.font_text(0, 6, 0x00FF008C, w2, h2 - 175, name);
    const FontId f = canvas_.create_font(kPageFont);
    const FontId old = canvas_.select_font(f);
    const Colour bk = canvas_.bk_colour(kBlack);
    canvas_.bk_mode(1);
    canvas_.text_align(6);
    const Colour tc = canvas_.text_colour(kDepth < 8 ? 0x00C8FF00 : 0x02FF8080);
    const auto res = host_.lang_resource(5000, text_page_resource(edition_));
    // PatBlt(0, H/2 - 0x73, W, H/2 + 0x95, BLACKNESS) [BYTES seg15:0x6a16 seg15:0x6a1e seg15:0x6a27]
    canvas_.black(0, h2 - 115, area_.w, h2 + 149);
    const auto lines = text_page_lines(res, page_);
    for (int i = 0; i < kPageLines; ++i)
        canvas_.text_out(w2, i * 24 + h2 - 115, lines[static_cast<std::size_t>(i)]);
    canvas_.bk_colour(bk);
    canvas_.text_colour(tc);
    canvas_.select_font(old);
    canvas_.delete_font(f);
    canvas_.font_text(0, 6, kWhite, w2, (kPageLines + 1) * 24 + h2 - 115,
                      load_limit(host_.lang(2119), 0x3c));
}

void Intro::keys() {  // seg15:0x6b52
    const int w2 = area_.w / 2, h2 = area_.h / 2;
    canvas_.font_text(0, 6, 0x000000FF, w2, h2 - 200, load_limit(host_.lang(2120), 0x50));
    for (const KeyRow& r : key_rows()) {
        canvas_.font_text(0, 0, r.colour, w2 - 295, h2 + r.dy, load_limit(host_.lang(r.key_id), 0x3c));
        canvas_.font_text(0, 0, r.colour, w2 - 5, h2 + r.dy,
                          "- " + load_limit(host_.lang(r.desc_id), 0x3c));
    }
}

// ---------------------------------------------------------------- repaint and keys

void Intro::repaint(Tick now) {
    now_ = now;
    // seg11:0x15fc-0x1661: phases 1 and 2 end now; 3-5 get 15 s from now unless the deadline is
    // already DGROUP:0x1e50. Both read GetCurrentTime afresh.
    if (phase_ > 0 && phase_ < 6) {
        if (phase_ < 3) deadline_ = host_.now();
        else if (deadline_ != now_) deadline_ = host_.now() + 15000;
    }
    frame(now, true);  // seg11:0x166a
}

void Intro::key_up(std::uint32_t vk, Tick now) {
    now_ = now;
    switch (vk) {
    case 'S':  // seg11:0x7959
        sound(0x6e);
        sound(0x13);
        todo_once("New Demo dialog (ordinal 10 command 4), then D", "seg11:0x7961");
        break;
    case 'D': todo_once("demo game (D: seg10:0x38e0, then C)", "seg11:0x797a"); break;
    case 'C': todo_once("demo start (C: phase 5 and the demo-start path)", "seg11:0x7990"); break;
    case 'R':
        if (edition_ == 0 || edition_ == 1) {  // seg11:0x79d5
            sound(0x51);
            todo_once("Order dialog (ordinal 10 command 8)", "seg10:0x80a8");
        }
        break;
    case 0x1b:  // ESC: the game-start flow (shell.md "Game-start dialogs")
        if (phase_ < 6) todo_once("Restore Game dialog of the ESC flow (stage 3)", "seg11:0x7a02");
        break;
    case 0x20:  // Space [BYTES seg11:0x7aea-0x7b13]
        if (phase_ < 6 && !host_.calibrating()) deadline_ = now;
        break;
    default: break;
    }
}

}  // namespace is::intro
