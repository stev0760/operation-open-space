#include "desk/desk.h"

#include <algorithm>

#include "briefing/save_text.h"
#include "formats/savegame.h"

namespace is::desk {

// seg10:0x9672 [BYTES seg10:0x9698 seg10:0x96b2 seg10:0x96c7 seg10:0x96df seg10:0x96f4
// seg10:0x970b seg10:0x9720 seg10:0x9737 seg10:0x974c seg10:0x9754].
int meal_of(const Clock& c, int lunch, int breakfast, int coffee, int tea) {
    const int t = c.hour * 100 + c.minute + 1;
    const std::pair<int, int> keys[] = {{lunch, 3}, {breakfast, 4}, {coffee, 1}, {tea, 2}};
    for (const auto& k : keys)
        if (k.first < t && t < k.first + 30) return k.second;
    return 0;
}

// [BYTES seg15:0xf46 seg15:0xf57 seg15:0xf63 seg15:0xf6f seg15:0xf7b seg15:0xf86]
int meal_graphic(int meal, int r) {
    switch (meal) {
    case 1: return 272;
    case 2: return 274;
    case 3: return 278;
    case 4: return 2 * (35 + r % 2);
    default: return 0;
    }
}

// seg10:0xa332 [BYTES seg10:0xa37b-0xa38a seg10:0xa3e2 seg10:0xa61a-0xa625 seg10:0xa6bc
// seg10:0xa64c]. The joining of consecutive enabled groups with ", " or string 797 is not
// ported: the desk's strings never enable two groups in a row [OPEN seg10:0xa61a]. A %S
// string drops its "^x" markers ("Coffee^P") [OPEN seg10:0xa332: what the markers select].
std::string format(const std::string& f, const std::vector<Arg>& args, const Lookup& lang) {
    std::string out;
    std::size_t next = 0;
    bool on = true;
    auto take = [&]() -> Arg { return next < args.size() ? args[next++] : Arg(0); };
    for (std::size_t i = 0; i < f.size(); ++i) {
        const char c = f[i];
        if (c != '%' || i + 1 >= f.size()) {
            if (on) out += c;
            continue;
        }
        const char k = f[++i];
        switch (k) {
        case '<': on = take().i != 0; break;
        case '>': on = true; break;
        case 'N':
            if (on) out += "\r\n";
            break;
        case 'S': {
            const Arg a = take();
            if (!on) break;
            const std::string s = a.is_string ? a.s : lang(a.i);
            for (std::size_t j = 0; j < s.size(); ++j) {
                if (s[j] == '^') {
                    ++j;
                    continue;
                }
                out += s[j];
            }
            break;
        }
        case 's': {
            const Arg a = take();
            if (on) out += a.is_string ? a.s : std::to_string(a.i);
            break;
        }
        case 'd': {
            const Arg a = take();
            if (on) out += std::to_string(a.i);
            break;
        }
        default:
            if (on) out += k;
            break;
        }
    }
    return out;
}

// seg15:0xcb0 [BYTES seg15:0xcc7 seg15:0xcda seg15:0xd15 seg15:0xd1c seg15:0xd29 seg15:0xd35
// seg15:0xda9 seg15:0xdbd seg15:0xe02 seg15:0xe69 seg15:0xe6e seg15:0xe74 seg15:0xe94
// seg15:0xec2 seg15:0xed5 seg15:0xee4 seg15:0xeed]. State 0 only: the sound 0x4b and the
// 1550 ms wait of the other states are not reached.
void Desk::enter(Tick now, int W, int H, int play_h) {
    now_ = now;
    W_ = W;
    H_ = H;
    play_h_ = play_h;
    t_ = Typing{};
    built_ = false;
    rise_alive_ = false;
    if (host_.ini_int("ShowLaunch", 2) == 0) {  // DGROUP:0x9298 [BYTES seg15:0xcda]
        step_ = 9;
        return;
    }
    canvas_.palette_save(0, 1);         // seg10:0x6da6(0, 1)
    canvas_.palette_scheme(0x6d, 2);    // seg10:0x6e76(0x6d, 2)
    render_ = host_.render(100, W, H);  // seg10:0x9526 [BYTES seg15:0xda9]
    canvas_.scene(render_);
    auto slot = [&](int i) { return i < static_cast<int>(render_.slots.size()) ? render_.slots[i] : Rect{}; };
    monitor_ = slot(0);    // DGROUP:0x3ca8 [BYTES seg15:0xd44]
    tower_ = slot(1);      // DGROUP:0x3cb8 [BYTES seg15:0xdeb]
    meal_rect_ = slot(4);  // DGROUP:0x3cd4 [BYTES seg15:0xe02]
    desk_bottom_ = std::max(slot(2).b, slot(3).b);  // DGROUP:0x92de [BYTES seg15:0xe66]
    screen_h_ = monitor_.b - monitor_.t - 15;       // DGROUP:0x92d2 [BYTES seg15:0x8611]
    canvas_.monitor(monitor_);  // seg15:0x8604
    canvas_.tower(tower_);      // seg15:0x8ae6
    // seg15:0xf2e: the meal [BYTES seg15:0xf2e seg15:0xf3b seg15:0xf41]
    meal_ = meal_of(host_.clock(), host_.ini_int("Lunch", 1200), host_.ini_int("Breakfast", 800),
                    host_.ini_int("Coffee", 1000), host_.ini_int("Tea", 1530));
    if (meal_ != 0) canvas_.meal(meal_graphic(meal_, meal_ == 4 ? host_.rand() : 0), meal_rect_);
    canvas_.scene_to_screen();  // [BYTES seg15:0xec2]
    step_ = 0;                  // [BYTES seg15:0xed5]
    wait_ = now + 1100;         // DGROUP:0x3cc4 [BYTES seg15:0xee4]
    built_ = true;
}

// seg15:0x52e4 [BYTES seg15:0x5313 seg15:0x5338-0x5394 seg15:0x53d2 seg15:0x53e5 seg15:0x56ef
// seg15:0x5700 seg15:0x572b seg15:0x5732 seg15:0x5737 seg15:0x5741 seg15:0x5752 seg15:0x575d
// seg15:0x576f seg15:0x577a].
std::string Desk::monitor_text() {
    const Lookup lang = [this](int id) { return host_.lang(id); };
    const Clock c = host_.clock();
    int part = day_part(c.hour);
    const std::string name = host_.ini("ThePlayer", "");
    const int p = part == 2 ? -41 : part;  // [BYTES seg15:0x535d]: 639 − 41 = 598 "Night"
    std::string buf = format(host_.lang(3032), {part != 3 ? 1 : 0, 591 + c.wday, 639 + p, part == 3 ? 1 : 0, 642, name}, lang);
    const std::string restore = host_.ini("Restore", "");
    if (restore.find(' ') != std::string::npos) {
        buf += host_.lang_text(5000, 210);  // " NEW GAME" [BYTES seg15:0x53d2 seg15:0x53e5]
    } else {
        // The restored game's progress lines: seg15:0x5405 opens LocalPath + Games + Restore +
        // ".sav" and returns before the mission text when it does not open; seg15:0x54af reads
        // the header without testing the read, and the lines are strings 3000, 3001 and 3031
        // (briefing/save_text.h desk_progress; formats.md "The desk monitor"; shell.md item 7)
        // [BYTES seg15:0x53e2-0x5405 seg15:0x5449 seg15:0x54af seg15:0x54b7 seg15:0x5601-0x562c]
        // (M4 s4 fixer B: save.md DIFFERENT 3).
        std::optional<std::vector<std::uint8_t>> header = host_.saved_header(restore);
        if (!header) return {};
        // A file shorter than the header leaves the rest of the buffer as it was [OPEN
        // seg15:0x54af]; the port reads zeros there [DESIGN].
        header->resize(fmt::save::kHeaderSize, 0);
        buf += briefing::desk_progress(header->data(), lang);
    }
    const int m = meal_;
    const int food = m == 1 ? 235 : m == 2 ? 236 : 648;
    buf += format(host_.lang(3030), {m != 3 ? 1 : 0, m == 3 ? 1 : 0, m == 0 ? 1 : 0, m != 0 ? 1 : 0, food, name}, lang);
    for (char& ch : buf)  // seg11:0x3fd4(buf, 1)
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    return buf;
}

// seg15:0x10be [BYTES seg15:0x1307 and the case bodies seg15:0x10d1-0x12e2].
bool Desk::frame(Tick now) {
    now_ = now;
    switch (step_) {
    case 0:
        host_.music(-1);  // [BYTES seg15:0x10dc]
        if (host_.now() > wait_) {
            const std::string text = monitor_text();
            if (!text.empty()) {
                t_.text = text;
                type(1);  // [BYTES seg15:0x577a]
            }
            ++step_;
        }
        break;
    case 1:
    case 4: type(0); break;
    case 2: {
        host_.sound(0x4b);  // [BYTES seg15:0x110a]
        const Tick t = host_.now() + 1460;
        host_.pump_until(t - 400);
        const int k = 4 * play_h_ / 3 - 1100;  // seg15:0x482e [BYTES seg15:0x483d seg15:0x48c2]
        pad_w_ = k / 36 + 164;
        pad_c_ = k / 30 + 80;
        canvas_.make_pad(pad_w_, pad_c_ + 52);
        host_.pump();
        size_ = canvas_.make_ship(host_.ini("MainShip", "EAGLE"));  // seg15:0x9e6, seg15:0x2a08
        host_.pump_until(t);
        rise(true);
        ++step_;
        break;
    }
    case 3: rise(false); break;
    case 5: {
        canvas_.screen_to_scene(monitor_.l + 29, monitor_.t + 29, monitor_.w() - 58, screen_h_ - 58);
        host_.busy_until(host_.now() + 800);  // [BYTES seg15:0x11df]
        launch(2);
        wait_ = host_.now() + 950;  // [BYTES seg15:0x120e]
        ++step_;
        break;
    }
    case 6:
        if (host_.now() > wait_) {
            ++step_;
            launch(1);
        }
        break;
    case 7:
        if (launch(0) != 0) {
            host_.pump();
            ++step_;
        }
        break;
    case 8:
        host_.pump();
        ++step_;
        break;
    default:
        end();
        return true;
    }
    return false;
}

// Step 9 [BYTES seg15:0x125b seg15:0x1263 seg15:0x1268 seg15:0x1286 seg15:0x12aa seg15:0x12b3
// seg15:0x12bd seg15:0x1328 seg15:0x1342 seg15:0x13a8 seg15:0x14be].
void Desk::end() {
    host_.pump();
    const bool early = canvas_.pad_alive() && size_ != 0;
    canvas_.free_all();
    if (early) host_.busy_until(host_.now() + 1300);  // seg15:0x1328 after an early ESC
    canvas_.black(0, 0, W_, H_);
    host_.pump_until(host_.now() + 450);
    canvas_.palette_scheme(8, 0);
    // seg10:0x6dd4(0) restores the set the entry's seg10:0x6da6(0, 1) saved; with ShowLaunch
    // = 0 the entry saved nothing and the port skips it [DESIGN].
    if (built_) canvas_.palette_restore(0);
    built_ = false;
    step_ = 9;
}

void Desk::space(Tick now) {
    now_ = now;
    if (step_ > 0 && step_ < 2) type(1);
}

// seg15:0x1584 [BYTES seg15:0x1592 seg15:0x15ae seg15:0x15b1 seg15:0x15d4 seg15:0x15ec
// seg15:0x1605 seg15:0x1615 seg15:0x1622 seg15:0x165e seg15:0x1665 seg15:0x1687 seg15:0x16b6
// seg15:0x1710 seg15:0x173b seg15:0x174d seg15:0x17a4 seg15:0x17cb seg15:0x1815 seg15:0x183a
// seg15:0x1846 seg15:0x1872 seg15:0x1882 seg15:0x18b5 seg15:0x18e4].
void Desk::type(int arg) {
    Typing& t = t_;
    if (arg == 1) {
        if (step_ == 0) {
            const int fx = W_ / 2, fy = monitor_.t + screen_h_ / 2;
            host_.sound_at(0x14, fx, fy, 4);  // seg15:0x84b0
            canvas_.flash(fx, fy);
            t.h = (monitor_.r - monitor_.l - 63) / 15;
            t.w = 8 * t.h / 21;
            canvas_.text_font(t.h, t.w);
            t.index = 0;
            t.x = t.cur_x = monitor_.l + 29;
            t.y = t.cur_y = monitor_.t + 29;
            t.line = 0;
            t.next = now_ + 500;
            t.started = true;
            return;
        }
        // Space at step 1: on to the CR whose index + 2 holds a space, and step 2.
        for (std::size_t i = t.index; i < t.text.size(); ++i) {
            if (t.text[i] == '\r' && i + 2 < t.text.size() && t.text[i + 2] == ' ') {
                t.index = i;
                break;
            }
        }
        step_ = 2;
        return;
    }
    if (!t.started || now_ < t.next) return;
    t.next = now_ + 40;
    const int ch = t.h - 10 * t.h / 13, cw = 6 * t.h / 13, chh = 10 * t.h / 13;
    for (int n = 0; n < 2; ++n) {
        if (t.index >= t.text.size()) {  // the NUL
            if (step_ == 4) step_ = 5;
            return;
        }
        char c = t.text[t.index];
        const signed char sc = static_cast<signed char>(c);
        if (sc < 13 && c != 0 && c != '\n') c = '\r';
        if (c == '\n') {
            ++t.index;
            continue;
        }
        if (c == '\r') {
            const std::size_t i = t.index;
            host_.sound_at(0x2b, 0, 0, 0);
            newline();
            ++t.index;
            if (t.index < t.text.size() && t.text[t.index] == '\n') ++t.index;
            if (i > 0 && t.text[i - 1] == '\n') {  // a CR after an LF: a blank line
                t.next = now_ + 1900;
                if (step_ == 1 && i + 2 < t.text.size() && t.text[i + 2] == ' ') step_ = 2;
                return;
            }
            if (step_ == 1 && i + 2 < t.text.size() && t.text[i + 2] == ' ') {
                step_ = 2;
                return;
            }
            continue;
        }
        if (c == ' ') {
            // The next word without its delimiter; the never-written byte the original measures
            // in front of it is left out [OPEN seg15:0x1710].
            std::size_t e = t.index + 1;
            while (e < t.text.size() && t.text[e] != ' ' && t.text[e] != '\r' && t.text[e] != '\n') ++e;
            const int extent = canvas_.text_extent(t.text.substr(t.index + 1, e - t.index - 1));
            if (monitor_.r - 25 - 9 < t.x + extent) {  // DGROUP:0x6088 - 9 [BYTES seg15:0x173b]
                host_.sound_at(0x2b, 0, 0, 0);
                newline();
                ++t.index;
                continue;
            }
        }
        if (t.index != 0) canvas_.black(t.cur_x + 2, t.cur_y + ch, cw, chh);
        canvas_.text_char(t.x, t.y, c);
        t.x += canvas_.text_extent(std::string(1, c));
        canvas_.block(t.x + 2, t.y + ch, cw, chh, 0x0000FF80);
        t.cur_x = t.x;
        t.cur_y = t.y;
        host_.sound_at(0x2b, 0, 0, 0);
        ++t.index;
    }
}

void Desk::newline() {
    Typing& t = t_;
    ++t.line;
    t.x = monitor_.l + 29;
    if (t.h + 1 > 0 && t.line < (screen_h_ - 58) / (t.h + 1)) {
        t.y += t.h + 1;
    } else {
        const Rect text{monitor_.l + 25, monitor_.t + 25, monitor_.r - 25, monitor_.t + screen_h_ - 25};
        canvas_.scroll_up(text, t.h + 1);
        t.cur_y -= t.h + 1;
    }
}

// seg15:0x4c76(first, 0) in state 0 [BYTES seg15:0x4c76 seg15:0x4cf2 seg15:0x4d0f seg15:0x4dbf
// seg15:0x4eb8 seg15:0x51ed seg15:0x525d seg15:0x527f seg15:0x5283].
bool Desk::rise(bool first) {
    if (first) {
        host_.sound(5);
        s4_ = 4 * size_;
        rise_off_ = s4_ + 0x3e;
        rise_alive_ = true;
    }
    if (!rise_alive_) return false;
    canvas_.rise(pad_w_, pad_c_ + 52, s4_, W_ / 2 - pad_w_ / 2, H_ - s4_ - 0x43 + rise_off_);
    if (first) return false;
    if (rise_off_ <= 5) {
        ++step_;
        canvas_.free_rise();
        rise_alive_ = false;
        return true;
    }
    rise_off_ = std::max(rise_off_ - 2, 5);
    return false;
}

std::array<ShipFrame, kShipFrames> ship_frames(int s) {
    std::array<ShipFrame, kShipFrames> out{};
    const int s4 = 4 * s;
    const int step = (s4 + (-54 * s) / 100) / 15;
    int w = s4, y = 0;
    out[0] = {s4, s4, 0};
    for (int i = 1; i < kShipFrames; ++i) {
        w -= step;
        const int p = i <= 9 ? 101 - 6 * i : 41 + 13 * (i - 10);
        y += out[i - 1].h;
        out[i] = {w, w * p / 100, y};
    }
    return out;
}

int launch_frame_of(int y0, int ye, int pos10, int prev_f) {
    const int span = (y0 - ye) / 2;
    int f;
    if (prev_f < 10) {
        const int d = std::max(1, span / 10);
        f = (y0 - pos10 / 10) / d;
    } else {
        const int d = std::max(1, span / 6);
        f = ((ye - y0) / 2 - pos10 / 10 + y0) / d + 10;
    }
    return std::min(f, 15);
}

int launch_speed(int f, bool fast) { return std::max(9, fast ? 150 - 4 * f : 150 - f / 2); }

int launch_advance10(int speed, bool fast) { return (fast ? 3 : 5) * speed / -7; }

// seg15:0x0(arg) in state 0 [BYTES seg15:0x58 seg15:0xc3 seg15:0xce seg15:0x14f seg15:0x185
// seg15:0x18b-0x221 seg15:0x25d seg15:0x262 seg15:0x272 seg15:0x290 seg15:0x2c4 seg15:0x598
// seg15:0x5b6 seg15:0x5cb seg15:0x613 seg15:0x6c3 seg15:0x6db seg15:0x6ea seg15:0x72f
// seg15:0x74f].
int Desk::launch(int arg) {
    if (arg == 2) {
        host_.sound(0x20);
        const int px = W_ / 2 - pad_w_ / 2;  // DGROUP:0x3cb0
        const int py = H_ - pad_c_ - 0x34;   // DGROUP:0x3cb2
        canvas_.pad_to_screen(px, py);
        start_y_ = py - s4_ + pad_c_ - 10;   // DGROUP:0x92d8 = H − s4 − 62
        end_y_ = 60;                         // DGROUP:0x92d6
        pos10_ = start_y_ * 10;
        speed_ = 150;
        f_ = 0;
        lum_ = 100;
        ship_rect_ = {};
    } else if (arg == 1) {
        host_.sound(0x1c);
    }
    const int f = launch_frame_of(start_y_, end_y_, pos10_, f_);
    if (f > f_) {  // the fade [BYTES seg15:0x25d-0x2b0 seg10:0x9956]
        const int old = lum_ == 0 ? 1 : lum_;
        lum_ = 100 - 100 * f / 40;
        const int p = 100 * lum_ / old;
        canvas_.palette_fade(3, p);
        canvas_.palette_fade(2, (p + 200) / 3);
    }
    f_ = f;
    const bool fast = host_.calibrate_value() >= 1400;
    speed_ = launch_speed(f, fast);
    // The drawing at the position before the step (item 9, Drawing).
    LaunchDraw d;
    d.f = f;
    d.s = size_;
    const int y = pos10_ / 10;
    d.ship = {W_ / 2 - 2 * size_, y, W_ / 2 - 2 * size_ + s4_, y + s4_ + 15};
    d.scene = true;  // desk phase 6 (DGROUP:0x95b8)
    d.column = {monitor_.l, monitor_.t, monitor_.r, desk_bottom_};
    const int px = W_ / 2 - pad_w_ / 2, py = H_ - pad_c_ - 0x34;
    d.pad = {px, py, px + pad_w_, py + pad_c_ + 52};
    canvas_.launch_frame(d);
    ship_rect_ = d.ship;
    pos10_ += launch_advance10(speed_, fast);
    if (10 * end_y_ > pos10_) {
        // The oracle shows the ship at the top for 1.43 s before the second restore; how that time
        // splits among the first restore, the ordinal-9 sound call and the flash is [OPEN]
        // (shell.md item 9, End of the rise). The oracle's 0x4c starts that time (sound-0001:
        // 0x4c, then 3.3 s to 0x4d) [ORACLE sound-0001 main], so the port plays it first and
        // then holds the shown frame (with the ship, as the oracle's screen keeps it) before the
        // first restore, the flash and the second restore [OPEN seg15:0x5cb
        // seg15:0x613 seg15:0x6db] [ORACLE start-0001 main] (M7 fixer).
        host_.sound_at(0x4c, 0, 0, 0);
        host_.busy_until(host_.now() + kLaunchEndHold);
        const int x = monitor_.l + 29, y = monitor_.t + 29, w = monitor_.w() - 58, h = screen_h_ - 58;
        canvas_.scene_to_screen(x, y, w, h);
        // seg15:0x84b0 silent at step 7 [BYTES seg15:0x84bf]
        canvas_.flash(ship_rect_.l + (ship_rect_.r - ship_rect_.l) / 2, ship_rect_.t + 12);
        canvas_.scene_to_screen(x, y, w, h);
        host_.pump_until(host_.now() + 1460);
        return 1;
    }
    return 0;
}

}  // namespace is::desk
