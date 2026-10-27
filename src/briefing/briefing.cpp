#include "briefing/briefing.h"

#include <algorithm>
#include <cstdio>

namespace is::briefing {

namespace {

// The byte before the measured word in the two typers' word measure is a never-written stack
// byte (bp - 0x44) [BYTES seg9:0x909b seg17:0x2c5a]. The port measures a space there [OPEN:
// shell.md "Open questions", the stack byte's value].
constexpr char kUnknownByte = ' ';

std::string cut_at_nul(const std::vector<std::uint8_t>& bytes, std::size_t max) {
    std::string s;
    for (std::size_t i = 0; i < bytes.size() && i < max; ++i) {
        if (bytes[i] == 0) break;
        s.push_back(static_cast<char>(bytes[i]));
    }
    return s;
}

// wsprintf of a format with one %d [DESIGN: the first "%d" is replaced, other text kept].
std::string format_d(const std::string& f, int v) {
    const auto p = f.find("%d");
    if (p == std::string::npos) return f;
    return f.substr(0, p) + std::to_string(v) + f.substr(p + 2);
}

// seg3:0x1a1d with upper = 1: strips one trailing space and raises a-z [BYTES seg3:0x1a1d].
std::string upper_name(std::string s) {
    if (!s.empty() && s.back() == ' ') s.pop_back();
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}

}  // namespace

// ---- geometry

Box box_for(int W, int H) {
    // seg9:0x8943-0x8a25: bw and bh, then the corners with truncating halvings.
    const int bw = W < 640 ? W + 20 : std::min(W - 20, 620);
    const int bh = H < 440 ? H + 20 : std::min(H - 20, 460);
    Box b;
    b.L = W / 2 - bw / 2;
    b.R = b.L + bw;
    b.T = H / 2 - bh / 2;
    b.bottom = b.T + bh;
    b.Y = b.bottom - 180;  // with the unclamped T
    if (H > 400 && b.T < 15) b.T = 15;
    b.cL = b.L < 0 ? 0 : b.L;
    b.cR = W < b.R ? W : b.R;
    // seg9:0x8b85-0x8bb0: the page and the clip region.
    b.pl = b.L + 6;
    b.pr = b.R - 6;
    b.pt = b.T + 11;
    b.pb = b.Y + 9;
    return b;
}

std::vector<Bevel> box_bevels(const Box& b) {
    return {
        {2, b.L, b.T - 15, b.R, b.Y + 180},    // the frame
        {-1, b.cL, b.T - 15, b.cR, b.T + 10},  // the title bar
        {-1, b.cL, b.Y + 20, b.cR, b.Y + 40},  // the status bar
    };
}

int text_resource(const WaveFlags& f, int wave) {
    // seg9:0x5ee7-0x5f46: 102, 103, 104 by flag, 101 for the first wave, else generated.
    if (f.f92e8) return 102;
    if (f.f94e) return 103;
    if (f.f950) return 104;
    if (wave == 0) return 101;
    return 0;
}

GeneratedPlan generated_plan(const GeneratedInputs& in) {
    GeneratedPlan p;
    // Step 1 [BYTES seg9:0x91b0].
    switch (in.kind) {
    case 0: p.lead = 1189; break;
    case 2: p.lead = in.byte53 == 2 ? 1998 : 1190; break;
    case 3: p.lead = 1192; break;
    case 4: p.lead = 1191; break;
    case 5: p.lead = 1193; break;
    default: break;
    }
    p.lead_space = in.kind != -1;
    // Step 2: DGROUP:0x933c += DGROUP:0x933a, DGROUP:0x933a = 0, and the ranking.
    const int v933c = in.v933c + in.v933a;
    const int v[6] = {0, 0, in.v933e, in.v9340, in.v4ee0, in.v4ee2};
    int count = 0, slot[3] = {-1, -1, -1}, max[3] = {0, 0, 0};
    for (int j = 0; j < 3; ++j) {
        for (int c = 0; c < 6; ++c) {
            bool ok = true;
            if (j > 0) ok = c != slot[0];
            if (j > 1 && c == slot[1]) ok = false;
            if (ok && max[j] < v[c]) {
                count = j + 1;
                slot[j] = c;
                max[j] = v[c];
            }
        }
    }
    // Step 3: rocks.
    if (count < 3) {
        if (!(v933c < 16 && (count != 0 || v933c < 1))) slot[count++] = 1;
    } else if (!(slot[2] == 3 || v933c <= max[2] || v933c < 16)) {
        slot[2] = 1;
    }
    // Step 4: the %D arguments.
    Slot args[3] = {{0, 0}, {0, 0}, {0, 0}};
    for (int j = 0; j < count; ++j) {
        switch (slot[j]) {
        case 1: args[j] = {3, 0}; break;
        case 2: args[j] = {8, 2 * in.v9318}; break;
        case 3: args[j] = {9, 0}; break;
        case 4: args[j] = {8, 2 * in.v9318 + in.v933e}; break;
        case 5: args[j] = {8, 2 * in.v9318 + in.v4ee6 + in.v4ee4 + in.v4ee0}; break;
        default: break;
        }
    }
    if (args[0].type == 0 && args[1].type == 0 && args[2].type == 0) {
        p.body = 1189;
    } else {
        p.body = 3036;
        for (int j = 2; j >= 0; --j)
            if (args[j].type != 0) p.slots.push_back(args[j]);
    }
    // Step 6.
    p.roster = in.roster_count;
    return p;
}

std::string count_text(Strings& s, int n, int id) {
    return std::to_string(n) + " " + s.lang(n == 1 ? id : id + 10000);
}

std::string d_text(Strings& s, const Slot& slot, bool* todo) {
    // Code 7 adds 10000 to the string id [BYTES seg10:0xa787 seg3:0x155c].
    switch (slot.type) {
    case 3: return upper_name(s.lang(10153));
    case 9: return upper_name(s.lang(10170));
    default:
        if (todo) *todo = true;  // type 8: the ship-class tables of M3 [OPEN]
        return {};
    }
}

std::string generated_text(Strings& s, const GeneratedPlan& p, bool* todo) {
    std::string t;
    if (p.lead) t += s.lang(p.lead);
    if (p.lead_space) t += " ";  // DGROUP:0xa5f
    if (p.body == 3036) {
        std::vector<std::string> names;
        for (const Slot& sl : p.slots) names.push_back(d_text(s, sl, todo));
        std::string list;
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (i > 0) list += i + 1 == names.size() ? s.lang(797) + " " : std::string(", ");
            list += names[i];
        }
        std::string f = s.lang(3036);
        const auto a = f.find("%<");
        const auto b = f.find("%>");
        if (a != std::string::npos && b != std::string::npos && b > a)
            f = f.substr(0, a) + list + f.substr(b + 2);
        t += f;
    } else if (p.body) {
        t += s.lang(p.body);
    }
    if (p.roster) {
        std::string f = s.lang(3037);
        const auto a = f.find("%#");
        if (a != std::string::npos) f = f.substr(0, a) + count_text(s, p.roster, 2538) + f.substr(a + 2);
        t += f;
    }
    return t;
}

std::vector<Label> box_labels(Strings& s, const Box& b, int wave, const std::string& directory,
                              int count) {
    // seg9:0x8a95-0x8b82; the formatter writes into a 120-byte buffer (bp - 0x7c).
    std::vector<Label> v;
    v.push_back({b.cL + 5, b.T - 13, 22, 10, kTaLeft, format_d(s.lang(2537), wave)});
    v.push_back({b.cR - 7, b.T - 6, 14, 0, kTaRight, s.lang(2268)});
    v.push_back({b.cL + 5, b.Y + 22, 15, 8, kTaLeft, directory.substr(0, 119)});
    if (count != 0) {
        std::string f = s.lang(2316);  // "%#"
        const auto a = f.find("%#");
        if (a != std::string::npos) f = f.substr(0, a) + count_text(s, count, 2515) + f.substr(a + 2);
        v.push_back({b.cR - 5, b.Y + 22, 15, 8, kTaRight, f});
    }
    return v;
}

// ---- drawing helpers

void draw_bevel(Canvas& c, int p, int l, int t, int r, int b) {
    const bool filled = p == -1;
    if (filled) p = 2;
    constexpr Colour kShade = 0x02808080, kLight = 0x02C0C0C0;
    constexpr Colour kBlackPen = 0x00000000, kWhitePen = 0x00FFFFFF;
    c.line(kShade, l, b, r, b);
    c.line(kShade, l + 1, b - 1, r - 1, b - 1);
    c.line(kShade, r - 1, b - 1, r - 1, t);
    c.line(kShade, l + p + 2, b - p - 3, l + p + 2, t + p + 2);
    c.line(kShade, l + p + 2, t + p + 2, r - p - 2, t + p + 2);
    if (p == 4) c.line(kShade, r, b, r, t);
    if (p != 4) c.line(kBlackPen, r, b, r, t);  // GetStockObject(BLACK_PEN)
    // GetStockObject(WHITE_PEN)
    c.line(kWhitePen, l + p + 2, b - p - 2, r - p - 2, b - p - 2);
    c.line(kWhitePen, r - p - 2, b - p - 2, r - p - 2, t + p + 1);
    c.line(kWhitePen, l, b, l, t);
    c.line(kWhitePen, l, t, r, t);
    c.line(kWhitePen, l + 1, b - 1, l + 1, t + 1);
    c.line(kWhitePen, l + 1, t + 1, r - 1, t + 1);
    // `p` nested Rectangles, NULL_BRUSH or LTGRAY_BRUSH for width -1.
    for (int i = 0; i < p; ++i) c.rectangle(kLight, filled, l + i + 2, t + i + 2, r - i - 1, b - i - 1);
    c.set_pixel(r, t, kShade);
    c.set_pixel(r - 1, t + 1, kLight);
    c.set_pixel(l, b, kLight);
    c.set_pixel(l + 1, b - 1, kLight);
}

void draw_label(Canvas& c, const Label& l) {
    const bool thirteen = l.height == 13;
    const Font f{thirteen ? 14 : l.height, l.width, 600};
    if (f.height > 13 && !thirteen) c.text(f, 0x02FFFFFF, l.align, l.x + 2, l.y + 2, l.text);
    c.text(f, 0x025A5A5A, l.align, l.x, l.y, l.text);
}

// ---- the typewriter seg9:0x8d18
//
// Rate [DESIGN]: both typers run one step on the first frame at or after their gate and re-arm
// it to now + 50 ms (docs/re/shell.md "Screen states", State 3, Page typer, **Rate**). The port
// keeps that gate as the only pace, so a step takes max(50 ms, one frame) on every machine: at
// the port's state-3 cadence of 72 Hz (frame_scheduler.h) a step is the fourth frame after the
// last, 55.6 ms, against the 64 ms of the Wine oracle's 16-ms frames (newgame-0002).

int Typewriter::step(Tick now, int reset, int instant) {
    // DGROUP:0x9460 (the help window) is 0 in state 3: the briefing frame does not run while it
    // is open.
    if (reset) {  // seg9:0x8d26
        x_ = left_ + 25;
        y_ = top_ + 55;
        if (!instant) crt_flash(c_, h_, x_ + 10, y_ + 10);  // [BYTES seg9:0x8d60]
        gate_ = now + 500;
        done_ = 0;
        index_ = 0;
        return 0;
    }
    int count = 2;
    if (!instant) {
        if (now < gate_) return done_;
        gate_ = now + 50;
    } else {
        count = 2000;
        c_.black(left_ + 6, top_ + 55, right_ - left_ - 12, bottom_ - top_ - 61);
    }
    if (done_ != 0) {
        // The blink [BYTES seg9:0x90e7].
        gate_ = now + 400;
        if (blink_ == 0)
            c_.black(x_ + 4, y_ + 2, 12, 15);
        else
            c_.fill(x_ + 4, y_ + 2, 12, 15, kColour);
        blink_ = blink_ == 0;
        return done_;
    }
    for (int n = 0; n < count; ++n) {
        bool wrap = false;
        if (at(index_) == ' ') {
            // seg9:0x8fc4: the next word with its delimiter.
            std::string word;
            int j = index_;
            char ch;
            do {
                ++j;
                ch = at(j);
                word.push_back(ch);
                if (ch == '\0' || ch == ' ') break;
            } while (ch != '\r');
            const int p3 = static_cast<int>(word.size());
            const std::string measured = kUnknownByte + word.substr(0, word.size() - 1);
            const int w = c_.text_width(kFont, measured);
            const int m = p3 < 6 ? 22 : 9;
            if (edge_ - m < w + x_) wrap = true;
        }
        if (at(index_) == '\r' || wrap) {
            x_ = left_ + 25;
            y_ += 25;
            if (at(index_) == '\r') ++index_;
        } else {
            if (index_ != 0) c_.black(cx_ + 4, cy_ + 2, 12, 15);
            const std::string ch(1, at(index_));
            c_.text(kFont, kColour, kTaLeft, x_, y_, ch);
            x_ += c_.text_width(kFont, ch);
            if (!instant) c_.fill(x_ + 4, y_ + 2, 12, 15, kColour);
            cx_ = x_;
            cy_ = y_;
        }
        ++index_;
        if (!instant) h_.sound_at(0x2b, cx_, cy_, 4);  // seg10:0x754a(0x2b, DGROUP:0x1834, DGROUP:0x1836, 4) (M7)
        if (at(index_) == '\r' && at(index_ - 1) == '\n') {
            gate_ = now + 2000;
            break;
        }
        if (at(index_) == '\0') {
            done_ = 1;
            break;
        }
    }
    return done_;
}

// ---- the page typer seg17:0x2c5a

int PageTyper::step(Tick now, int reset, int instant, int phase) {
    int count = 10;
    if (!reset) {
        if (now < gate_) return 0;  // seg17:0x2ced
        gate_ = now + 50;
    } else {
        if (!instant) crt_flash(c_, h_, ox_ + 15, oy_ + 10);  // [BYTES seg17:0x2c87]
        done_ = 0;
        index_ = 0;
        x_ = ox_;
        y_ = oy_;
        gate_ = now + 500;
        until_ = 0;
        if (!instant) return 0;
        count = 1000;
    }
    if (done_ == 0) {
        for (int n = 0; n < count; ++n) {
            bool wrap = false;
            if (wrap_ && at(index_) == ' ') {
                std::string word;
                bool tab_or_cr = false;
                int j = index_;
                char ch;
                do {
                    ++j;
                    ch = at(j);
                    word.push_back(ch);
                    if (ch == '\t' || ch == '\r') tab_or_cr = true;
                } while (ch != '\0' && ch != ' ' && ch != '\r');
                if (!tab_or_cr) {
                    const int p3 = static_cast<int>(word.size());
                    const std::string measured = kUnknownByte + word.substr(0, word.size() - 1);
                    const int w = c_.text_width(kFont, measured);
                    const int m = p3 < 6 ? 22 : 9;
                    if (edge_ - m < w + x_) wrap = true;
                }
            }
            if (at(index_) == '\r' || wrap) {
                x_ = ox_;
                y_ += 18;
                if (at(index_) == '\r') ++index_;
            } else if (at(index_) == '\t') {
                // TAB and three digits: x = DGROUP:0x5000 + the number.
                x_ = ((at(index_ + 1) * 10 + at(index_ + 2)) * 10 + at(index_ + 3)) + ox_ - 0x14d0;
                index_ += 3;
            } else {
                if (index_ != 0 && !instant) c_.black(cx_ + 4, cy_ + 2, 12, 18);
                const std::string ch(1, at(index_));
                c_.text(kFont, kColour, kTaLeft, x_, y_, ch);
                x_ += c_.text_width(kFont, ch);
                if (!instant) {
                    c_.fill(x_ + 4, y_ + 2, 12, 12, kColour);
                    cx_ = x_;
                    cy_ = y_;
                }
            }
            ++index_;
            if (!instant) {
                h_.sound(0x2b);  // seg10:0x744c(0x2b)
                if (at(index_) == '\r' && at(index_ - 1) == '\n' && phase != 15 && phase != 17) {
                    gate_ = now + 900;
                    break;
                }
            }
            if (at(index_) == '\0') {
                done_ = 1;
                break;
            }
        }
        if (done_ == 0) return 0;
    } else {
        gate_ = now + 400;
        if (until_ == 0) {
            until_ = hold_ + now;
            if (!instant) c_.black(x_ + 4, y_ + 2, 12, 12);
        }
    }
    return (until_ == 0 || now <= until_) ? 1 : 2;
}

int page_resource(int kind, int meal, int ship_page) {
    switch (kind) {
    case 2: case 8: case 9: case 11: case 12: case 16: case 402: return 0;  // [BYTES seg17:0x313a]
    default: break;
    }
    if (kind == 3) {
        switch (meal) {
        case 0: return 301;
        case 1: return 181;
        case 2: return 182;
        case 3: return 180;
        case 4: return 183;
        default: return 3;  // the id stays the kind
        }
    }
    if (kind < 110) {
        switch (kind) {
        case 15: return 320 + ship_page;
        case 17: return 137;
        case 4: return 302;
        case 5: return 303;
        case 7: return 305;
        case 6: return 304;
        default: return kind + 299;
        }
    }
    return kind;
}

bool page_wraps(int p) {
    return p == 2 || p == 3 || (p >= 8 && p <= 13) || (p >= 15 && p <= 17);
}

int slide_in_frames(int kind) {
    switch (kind) {
    case 3: return 0x96;
    case 7: return 0xbe;
    case 8: return 0x70;
    case 9: return 0x66;
    default: return 0;
    }
}

SlideKind slide_kind(int kind) {
    // The cells and gutters are the pool records' words +0x22 and +0x24, which the wave set-up
    // fills (M3). Kind 7's 40 and 6 are the logo strip's (start-0001); the others are the
    // port's stand-ins for the placeholder [OPEN: seg17:0x1c2e seg17:0x1c54].
    switch (kind) {
    case 3: return {kSlotRocks, 0x96, 0x12, 0x2c, 40, 6};
    case 7: return {kSlotStrip, 0xbe, 0x0a, 0x28, 40, 6};
    // Kind 8: slot word DGROUP:0x3d6e + 0x34c and base byte DGROUP:0x4d98 + 0x14 * word
    // DGROUP:0x3d80 + 0x34c, both M3 [OPEN: seg17:0x1be9]; the port uses base 40.
    case 8: return {kSlotShip, 0x70, 0x10, 40, 40, 6};
    case 9: return {kSlotViruses, 0x66, 0x09, 0x26, 37, 5};
    default: return {};
    }
}

void crt_flash(Canvas& c, Host& h, int x, int y) {
    h.sound_at(0x14, x, y, 4);  // seg10:0x754a(0x14, x, y, 4) [BYTES seg15:0x84d5] (M7)
    constexpr Colour kWhitePen = 0x00FFFFFF;
    // MoveTo/LineTo with the stock WHITE_PEN; LineTo stops before the end point.
    for (int i = 2; i < 200; ++i) {
        const int r = i / 10;
        c.line(kWhitePen, x, y - r, x, y + r);
        c.line(kWhitePen, x - r, y, x + r, y);
    }
    for (int i = 200; i > 0; --i) {
        const int r = i / 10;
        c.set_pixel(x, y - r, 0);
        c.set_pixel(x, y + r, 0);
        c.set_pixel(x - r, y, 0);
        c.set_pixel(x + r, y, 0);
        c.black(x - 1, y - 1, 2, 2);
    }
}

std::vector<GreetingLine> greeting_lines(Strings& s, int W, int H, int hour,
                                         const std::string& player, const std::string& drive) {
    const int part = day_part(hour);
    std::string first = part == 3 ? s.lang(590) : s.lang(311) + " ";
    first += s.lang(639 + part) + ", " + player + "!";
    return {
        {W / 2, H / 2 - 90, 0x0000FF80, first},
        {W / 2, H / 2 - 55, 0x0000FF80, s.lang(645)},
        {W / 2, H / 2, 0x0000FF00, s.lang(936) + drive},
    };
}

namespace {
// seg6:0xf54 / seg6:0x1010 for (value, 100, digits) as start-0001 shows them [OPEN].
std::string hundredths(long v, int digits) {
    const bool neg = v < 0;
    if (neg) v = -v;
    std::string t = std::to_string(v / 100) + ".";
    const long frac = v % 100;
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02ld", frac);
    t += digits >= 2 ? std::string(buf) : std::string(1, buf[0]);
    return neg ? "-" + t : t;
}
}  // namespace

std::string ship_page_text(Strings& s, int wave, const std::string& directory,
                           const ShipStats& st) {
    const std::string f = s.lang(3035);
    // The arguments in order [BYTES seg17:0x341c-0x347c]: max(wave, 1), the directory, the
    // score, the two counts with 2515, the credit and the resources.
    std::vector<std::string> args = {std::to_string(std::max(wave, 1)), directory,
                                     std::to_string(st.score),
                                     count_text(s, st.captured, 2515),
                                     count_text(s, st.destroyed, 2515),
                                     hundredths(st.credit, 2), hundredths(st.resources, 1)};
    std::string out;
    std::size_t a = 0;
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] == '%' && i + 1 < f.size()) {
            const char k = f[i + 1];
            if (k == 'N') {
                out += "\r\n";
                ++i;
                continue;
            }
            if (k == 'd' || k == 's' || k == 'l' || k == '#') {
                if (a < args.size()) out += args[a++];
                ++i;
                continue;
            }
        }
        out.push_back(f[i]);
    }
    return out;
}

// ---- the briefing

int Briefing::enter(shell::GameState& s, const Setup& setup) {
    const int id = text_resource(setup.flags, wave_);
    std::string text;
    if (id != 0) {
        text = cut_at_nul(h_.page(id), 0xffff);
    } else {
        // seg9:0x91b0 reads the wave set-up's counts (M3); the port builds it from zeros.
        h_.todo("generated briefing: the wave's counts", "seg9:0x91b0");
        bool todo = false;
        text = generated_text(h_, generated_plan(GeneratedInputs{}), &todo);
        if (todo) h_.todo("generated briefing: %D type 8 ship classes", "seg3:0x1817");
        if (text.size() > 1999) text.resize(1999);  // the 2000-byte block DGROUP:0x93a8
    }
    enter_text(s, setup, std::move(text));
    return id;
}

void Briefing::enter_text(shell::GameState& s, const Setup& setup, std::string text) {
    setup_ = setup;
    text_ = std::move(text);
    icons_left_ = 0;
    for (int k = 0; k < setup_.icons; ++k)
        if (k >= static_cast<int>(setup_.gone.size()) || !setup_.gone[k]) ++icons_left_;
    ++wave_;  // seg9:0x5fbf
    draw_box(s, 1);
}

void Briefing::draw_box(shell::GameState& s, int arg) {
    box_ = box_for(setup_.W, setup_.H);
    if (setup_.quick_intro == 0 && setup_.command_line == 0) {  // seg9:0x8a29 seg9:0x8a32
        for (const Bevel& b : box_bevels(box_)) draw_bevel(c_, b.width, b.l, b.t, b.r, b.b);
        for (const Label& l : box_labels(h_, box_, wave_, setup_.directory, setup_.icons))
            draw_label(c_, l);
    }
    typer_.set_text(text_);
    typer_.set_panel(box_.L, box_.Y, box_.R, box_.bottom, box_.pr);
    s.set_briefing_phase(0);  // seg9:0x8bcb
    run(s, arg == 0, 1);      // seg9:0x8bdc
}

void Briefing::repaint(shell::GameState& s) {
    if (s.briefing_phase() != shell::kBriefingLastPhase) draw_box(s, 0);
}

void Briefing::black_page() { c_.black(box_.pl, box_.pt, box_.pr - box_.pl, box_.pb - box_.pt); }

void Briefing::run(shell::GameState& s, int a, int b) {
    now_ = s.now();
    // Step 1 [BYTES seg17:0x14 seg17:0x1b seg17:0x29].
    if ((setup_.quick_intro != 0 || setup_.command_line != 0) &&
        s.briefing_phase() != shell::kBriefingLastPhase) {
        s.set_briefing_phase(shell::kBriefingLastPhase);
        return;
    }
    // Step 2 (DGROUP:0x946c, the ship index of phase 11, is not kept).
    bool space = false;
    if (s.briefing_skip() == -3) {  // only the help window sets -3 and -2
        black_page();
        h_.sound(0x6e);
        h_.todo("briefing ship page redraw seg17:0x20fa(1, 0, 1)", "seg17:0x20fa");
    }
    if (s.briefing_skip() == -2) advance(s);
    if (s.briefing_skip() == -1) {
        if (s.briefing_phase() == 11) {  // seg17:0x93-0x10b
            black_page();
            h_.sound(0x6e);
            // seg17:0x20fa(1, 1, 0): the next ship; past the last ship DGROUP:0x9316 it arms
            // DGROUP:0x18c2 = now + 100 and returns 1 [BYTES seg17:0x2283-0x22a4]. The port's
            // roster holds only the player's ship (M3), so that is the result [OPEN].
            ++ship_;
            fl_.end_at = now_ + 100;
            const int r = 1;
            if (s.briefing_skip() == -1) s.set_briefing_skip(0);
            if (r != 0) {
                s.set_briefing_phase(shell::kBriefingLastPhase);
                advance(s);
            }
        } else {
            space = true;
            b = 1;
            t1876_ = now_ - 10;
        }
    }
    // Step 3.
    if (b != 0) {
        int r;
        if (space) {
            r = typer_.step(now_, 0, 0);
        } else {
            typer_.step(now_, 1, a);
            r = a;
            if (s.briefing_phase() != 0) t1876_ = now_ - 10;
        }
        if (r == 0) {
            if (a != 0 || space) {
                typer_.step(now_, 1, 1);
                typer_.step(now_, 0, 1);
                t1876_ = now_ - 10;
            } else {
                s.set_briefing_phase(0);
            }
        }
    }
    // Step 4.
    const int done = typer_.step(now_, 0, 0);
    // Step 5.
    const int phase = s.briefing_phase();
    if (phase != 0) c_.clip(true, box_.pl, box_.pt, box_.pr, box_.pb);
    // Step 6.
    int adv = 0;
    if (phase == 0) {
        if (done != 0 && t1876_ == 0) t1876_ = now_ + 500;
    } else if (phase == shell::kBriefingLastPhase) {
        // seg17:0x332-0x343: DGROUP:0x5ac = now once, and seg17:0x6036.
        if (s.deadlines().briefing_end == 0) {
            s.deadlines().briefing_end = now_;
            // seg17:0x6036 only deletes the briefing's GDI objects and windows and sets
            // DGROUP:0x946c = -1 (shell.md "Clean-up"); the port's clip region and fonts are
            // released with the frame and the canvas, so nothing is left to do [BYTES
            // seg17:0x6036].
        }
        c_.clip(false, 0, 0, 0, 0);
        return;
    } else if (phase < shell::kBriefingLastPhase) {
        adv = routine(s, phase, 0);
    }
    // Step 7.
    if (t1876_ != 0 && t1876_ < now_) {
        t1876_ = 0;
        adv = 1;
    }
    if (adv != 0) {
        s.set_briefing_phase(s.briefing_phase() + 1);
        advance(s);
    }
    // The original's frame DC is released after each pass; the port's surface DC lives on, so
    // the clip region goes with the frame [DESIGN].
    c_.clip(false, 0, 0, 0, 0);
}

void Briefing::advance(shell::GameState& s) {
    // seg17:0x3b2 [BYTES seg17:0x3bf seg17:0x3c7 seg17:0x3cd seg17:0x3d4 seg17:0x3db].
    h_.sound(0x6e);
    s.set_briefing_skip(0);
    if (s.briefing_phase() > 17 && setup_.play_mode == 0) s.set_briefing_phase(1);
    black_page();
    const int p = s.briefing_phase();
    if (p >= 1 && p <= 17) routine(s, p, 1);
}

int Briefing::held(int init, const char* what, const char* address) {
    // A phase whose drawing is open: the port logs it at its init and advances at the next
    // frame [DESIGN].
    if (init) {
        h_.todo(what, address);
        return 0;
    }
    return 1;
}

void Briefing::slide_draw(const SlideKind& k, int x, int y, int sz, int sx, int sy, int sw) {
    if (c_.has_slot(k.slot)) {
        c_.slot_blit(k.slot, x, y, sz, sz, sx, sy, sw, sw);
    } else {
        // The slot's picture comes with the wave set-up (M3): the destination's outline
        // [OPEN: seg17:0x1cde].
        c_.black(x, y, sz, sz);
        if (sz > 1) c_.rectangle(kPlaceholder, false, x, y, x + sz, y + sz);
    }
}

int Briefing::slide_in(int init, int kind) {
    // seg17:0x1b3e(init, kind) [BYTES seg17:0x1b3e-0x20f8]. x uses W = DGROUP:0x608c (the
    // play-area width), y the page DGROUP:0x6086 / DGROUP:0x608a.
    const SlideKind k = slide_kind(kind);
    const int W = setup_.W;
    const int T = box_.pt, Yb = box_.pb;
    if (init) {
        h_.sound(0x6e);  // seg10:0x744c(0x6e), (0x13) [BYTES seg17:0x1b84 seg17:0x1b8c]
        h_.sound(0x13);
        t18b6_ = 0;
        n18ba_ = 1;
        a18c0_ = 1;
        n18be_ = 0;
    }
    if (t18b6_ != 0) return t18b6_ < now_ ? 1 : 0;  // [BYTES seg17:0x1ba4-0x1bb5]
    const int two = 2 * k.base;
    auto size_of = [&](int n) { return std::min(two, static_cast<int>(static_cast<std::int16_t>(k.base * n)) / 55); };
    if (sub() == 0) {
        // Growth [BYTES seg17:0x1c1e-0x1d4e].
        const int sz = size_of(n18ba_);
        if (sz < two) n18be_ = n18ba_;
        const int x = W / 2 - sz / 2 - n18be_ - 30;
        const int y = (T - Yb) / 2 - sz / 2 + Yb;
        slide_draw(k, x, y, sz, k.gutter + (k.gutter + k.cell) * (a18c0_ / 2), k.gutter, k.cell);
        c_.black(x + sz, y, 2, sz);
        ++n18ba_;
        if (2 * k.cells <= ++a18c0_) a18c0_ = 0;
        if (k.frames < n18ba_) {
            set_sub(1);
            pages_ox_ = sz + x + 0x5a;
            pages_oy_ = y - 10;
            load_page(cur_phase_, cur_phase_, 5000);
        }
    } else if (sub() == 1) {
        // Page [BYTES seg17:0x1e2c-0x1f74]: kinds 8 and 9 keep turning.
        if (kind != 3 && kind != 7) {
            const int sz = size_of(n18ba_);
            if (sz < two) n18be_ = n18ba_;
            const int x = W / 2 - sz / 2 - n18be_ - 30;
            const int y = (T - Yb) / 2 - sz / 2 + Yb;
            slide_draw(k, x, y, sz, k.gutter + (k.gutter + k.cell) * (a18c0_ / 2), k.gutter,
                       k.cell);
            c_.black(x + sz, y, 2, sz);
            if (2 * k.cells <= ++a18c0_) a18c0_ = 0;
        }
        const int r = pages_.step(now_, 0, 0, cur_phase_);
        if (r == 2) {
            h_.sound(0x15);  // seg10:0x744c(0x15)
            n18ba_ = n18bc_ = 110;
            set_sub(2);
        }
        // r == 1 calls seg10:0x565a, the meal slot, which draws nothing [BYTES seg17:0x1f46].
    } else if (sub() == 2) {
        // Shrink [BYTES seg17:0x1f77-0x20da].
        const int sz = std::min(two, static_cast<int>(static_cast<std::int16_t>(k.base * n18bc_)) / 55);
        if (sz < two) n18be_ = n18ba_;
        const int x = W / 2 - sz / 2 - n18be_ - 30;
        const int y = (T - Yb) / 2 - sz / 2 + Yb;
        slide_draw(k, x, y, sz, k.gutter - 2 + (k.gutter + k.cell) * (a18c0_ / 2), k.gutter - 2,
                   k.cell + 4);
        c_.black(x - 4, y, 4, sz);
        c_.black(x - 10, y - 10, sz + 10, 10);
        n18bc_ = n18ba_;
        n18ba_ -= 2;
        if (--a18c0_ < 0) a18c0_ = 2 * k.cells - 1;
        if (n18ba_ <= 8) {
            c_.black(x - 4, y - 4, sz + 8, sz + 8);
            t18b6_ = now_ + 1000;
        }
    }
    return 0;
}

int Briefing::race(int init) {
    // seg17:0x13ca, phase 2 [BYTES seg17:0x13ca seg17:0x144b seg17:0x5008 seg17:0x14fc
    // seg17:0x14d2].
    if (init) {
        t18a0_ = 0;
        return 0;
    }
    if (sub() == 0) {
        set_sub(sub() + 1);
        // seg17:0x5008: string 0x2e4 + DGROUP:0x9322 (the race, M3: 0) [OPEN].
        h_.todo("briefing race: DGROUP:0x9322 and the race picture seg10:0x8b1c", "seg17:0x13ca");
        c_.text(Font{15, 10, 0}, 0x02FFD600, kTaLeft, box_.pr - 0x118, box_.pt + 20,
                h_.lang(0x2e4));
        const int side = box_.pb - box_.pt;
        c_.rectangle(kPlaceholder, false, box_.pl, box_.pt, box_.pl + side, box_.pt + side);
        pages_ox_ = box_.pr - 0x118;
        pages_oy_ = box_.pt + 0x36;
        load_page(cur_phase_, 402, 3000);
    } else if (sub() == 1) {
        if (pages_.step(now_, 0, 0, cur_phase_) == 2) set_sub(sub() + 1);
    } else {
        if (t18a0_ == 0) t18a0_ = now_ + 1000;
        else if (t18a0_ < now_) return 1;
    }
    return 0;
}

int Briefing::defenders(int init) {
    // seg17:0x193c, phase 7 [BYTES seg17:0x194f seg17:0x1991 seg17:0x19b1 seg17:0x19ba
    // seg17:0x1a37 seg17:0x1a69 seg17:0x1a80 seg17:0x1aa5 seg17:0x1ab1 seg17:0x1b25].
    if (init) {
        t18b0_ = 0;
        return 0;
    }
    // seg17:0x55e2(-1): 12530 upper-cased, Arial 15 x 10, 0x020080FF.
    auto label = [&] {
        c_.text(Font{15, 10, 0}, 0x020080FF, kTaLeft, box_.pl + 40, box_.pt + 20,
                upper_name(h_.lang(12530) + " "));
    };
    // The pool-2 cell stretched to 13 x 13 with MERGECOPY and brush DGROUP:0x5780[i]: the
    // port fills the square with a grey of the pulse step [OPEN: the brushes and the cell].
    auto pulse = [&](int i) {
        const Colour grey = 0x02000000u | static_cast<Colour>(0x30 + 0x30 * i) * 0x010101u;
        c_.fill(box_.pr / 2 - 200, pages_oy_ + 50, 13, 13, grey);
    };
    if (sub() == 0) {
        set_sub(1);
        pages_ox_ = box_.pr - 0x140;
        pages_oy_ = box_.pt + 0x1e;
        load_page(cur_phase_, cur_phase_, 4500);
        label();
        t18b4_ = 0;
        h_.todo("briefing defenders: the pool-2 cell and brushes DGROUP:0x5780", "seg17:0x193c");
        pulse(0);
    } else if (sub() == 1) {
        label();
        t18b4_ = t18b4_ + 1 > 49 ? 0 : t18b4_ + 1;
        pulse(std::min(t18b4_, 49 - t18b4_) / 5);
        if (pages_.step(now_, 0, 0, cur_phase_) == 2) set_sub(2);
    } else {
        if (t18b0_ == 0) t18b0_ = now_ + 1000;
        else if (t18b0_ < now_) return 1;
    }
    return 0;
}

// ---- phase 11, seg17:0x20fa

namespace {
// The player's ship before M3 [OPEN: seg17:0x2404 seg17:0x24d7]: start-0001's TRANSFORMER, a
// 33-px cell with a 5-px gutter and size 33; m = 3, the value for DGROUP:0x90ee >= 8 (the
// port's 8-bit surface).
constexpr int kShipSize = 33, kShipCell = 33, kShipGutter = 5, kShipM = 3;
constexpr int kLastShip = 0;  // DGROUP:0x9316: the port's roster is the player's ship
}  // namespace

void Briefing::ship_setup() {
    // [BYTES seg17:0x22b9-0x23ee]
    auto& f = fl_;
    f = Flight{};
    f.motion = 1;
    f.width0 = 20;
    f.width_d = 80;
    f.scale0 = 25;
    f.scale_d = 100 * kShipM - 25;
    f.mid = (box_.pt - box_.pb) / 2 + box_.pb;
    f.cell0 = 400;
    f.px10 = (box_.pl + 40) * 10;
    f.end = (box_.pr - box_.pl) * 11 / 3 + f.px10;
    f.cx = static_cast<int>(f.px10 / 10);
    f.cy = static_cast<int>(f.mid);
    h_.sound(0x13);  // seg10:0x754a(0x13, 100, DGROUP:0x90ec/2, 4): played as seg10:0x744c until the briefing has DGROUP:0x90ec [OPEN] (M7)
    h_.todo("ship records: seg11:0xb09c, the class size and the slot (M3)", "seg11:0xb09c");
}

int Briefing::ship(int init) {
    auto& f = fl_;
    if (init) {
        // seg17:0x2260: DGROUP:0x91f8 + 1 around seg9:0x6624 (M3), ship 0.
        ship_ = 0;
        ship_setup();
    }
    if (f.end_at != 0) return f.end_at < now_ ? 1 : 0;  // [BYTES seg17:0x2405-0x241a]
    // One flight frame at `pos` (px10 for the fly-in, DGROUP:0x18fe for the fly-out).
    auto fly = [&](long pos, bool out) {
        const long scale = f.scale0 + (f.end ? f.scale_d * pos / f.end : 0);
        const long step = (scale / 2) * f.motion;
        const long width = f.width0 + (f.end ? pos * f.width_d / f.end : 0);
        const long k = f.frames == 0 ? f.cell0 / 100
                                     : (f.cell0 + f.cell_d * f.frame / f.frames) / 100;
        f.cy = static_cast<int>(f.mid + (f.end ? f.dy * pos / f.end : 0));
        const int h = static_cast<int>(kShipSize * scale / 100);
        f.y = f.cy - h / 2;
        const int w = static_cast<int>(h * width / 100);
        f.x = f.cx - w / 2;
        f.h = h;
        const int sx = kShipGutter + (kShipGutter + kShipCell) * static_cast<int>(k);
        if (c_.has_slot(kSlotShip)) {
            c_.slot_blit(kSlotShip, f.x, f.y, w, h, sx, kShipGutter, kShipCell, kShipCell);
        } else if (w > 0 && h > 0) {
            c_.black(f.x, f.y, w, h);
            c_.rectangle(kPlaceholder, false, f.x, f.y, f.x + w, f.y + h);
        }
        if (!out) {
            c_.black(f.x - 20, f.y - 20, 20, h + 20);  // [BYTES seg17:0x25a0]
        } else {
            c_.black(f.x - 20, f.y - 20, w + 20, 20);
            c_.black(f.x - 20, f.y + h, w + 20, 20);
            c_.black(f.x - 20, f.y - 20, 20, h + 20);
        }
        f.px10 += step;
        f.cx = static_cast<int>(f.px10 / 10);
        ++f.frame;
        return step;
    };
    if (sub() == 0) {
        // Fly-in and the 10-frame spin [BYTES seg17:0x2450-0x26b8].
        fly(f.px10, false);
        if (f.px10 >= f.end && f.frame >= f.frames) {
            ++f.pass;
            f.width0 = 100, f.width_d = 0;
            f.scale0 = 100 * kShipM, f.scale_d = 0;
            f.mid = f.cy, f.dy = 0;
            f.cell0 = 400, f.cell_d = -400;
            f.frame = 0, f.frames = 10;
            f.motion = 0;
            if (f.pass > 1) {
                // The page: the invisible spin's set-up, then seg17:0x30b0(11, hold).
                f.out = f.end;
                f.pass = 0;
                f.scale0 = 0;
                f.cell0 = 0, f.cell_d = 400;
                set_sub(1);
                pages_ox_ = f.h + f.x + 0x27;
                pages_oy_ = (box_.pt - box_.pb) / 2 + box_.pb - 0x56;
                const bool player = true;  // DGROUP:0x9464 = DGROUP:0x9646 [OPEN: M3 roster]
                const Tick hold = (wave_ != 0 && player) ? 3500 : 5000;
                ShipStats st;
                h_.todo("ship page: score, counts, credit and resources of the roster (M3)",
                        "seg17:0x33ee");
                h_.todo("ship page: seg17:0x4068's sentence and the allies/enemies lines",
                        "seg17:0x4068");
                std::string text = ship_page_text(h_, wave_, setup_.directory, st);
                pages_.set_page(std::move(text), page_wraps(cur_phase_), hold);
                pages_.set_origin(pages_ox_, pages_oy_, box_.pr);
                pages_.step(now_, 1, 0, cur_phase_);
                ship_labels();
            }
        }
    } else if (sub() == 1) {
        // The page [BYTES seg17:0x27c5-0x2836].
        const int r = pages_.step(now_, 0, 0, cur_phase_);
        if (r == 2) {
            set_sub(sub() + 1);
            c_.black(pages_ox_, pages_oy_, box_.pr - pages_ox_, box_.pb - pages_oy_);
            c_.black(box_.pl, pages_oy_, pages_ox_ - 100, box_.pb - pages_oy_);
        }
    } else if (sub() == 2) {
        // The invisible spin and the fly-out [BYTES seg17:0x283e-0x2c38].
        const long step = fly(f.out, true);
        f.out += step;
        if (f.out >= f.end && f.frame >= f.frames) {
            f.motion = 1;
            f.width0 = 100, f.width_d = -80;
            f.scale0 = 100 * kShipM, f.scale_d = 50 - 100 * kShipM;
            f.mid = f.cy, f.dy = 0;
            f.cell0 = 400, f.cell_d = 0;
            f.out = 0;
            f.end = box_.pr * 10L - f.px10 - 200;
            f.frames = f.frame = 0;
            ++f.pass;
            if (f.pass == 1) {
                h_.sound(0x15);  // seg10:0x754a(0x15, DGROUP:0x6088/2 + 250, ...): played as seg10:0x744c, the y is unread [OPEN] (M7)
            } else {
                black_page();
                if (++ship_ > kLastShip) {
                    f.end_at = now_ + 1000;
                } else {
                    set_sub(0);
                    ship_setup();
                }
            }
        }
    }
    return 0;
}

void Briefing::ship_labels() {
    // seg17:0x4354 for the player's ship [BYTES seg17:0x4354]: Arial 15 x 10 weight 0.
    const int x = pages_ox_ - 0x14a;
    const Font f{15, 10, 0};
    c_.text(f, 0x02FFD600, kTaLeft, x, pages_oy_, h_.lang(523));
    // 2522 "(%<this is you%>%<from %? team%>)" with the first group on [OPEN: the line's
    // colour is the first line's].
    std::string t = h_.lang(2522);
    const auto a = t.find("%<"), b = t.find("%>");
    if (a != std::string::npos && b != std::string::npos && b > a) {
        const std::string first = t.substr(a + 2, b - a - 2);
        const auto c = t.find("%<", b), d = t.find("%>", b + 2);
        std::string rest = c != std::string::npos && d != std::string::npos ? t.substr(d + 2) : t.substr(b + 2);
        t = t.substr(0, a) + first + rest;
    }
    c_.text(f, 0x02FFD600, kTaLeft, x, pages_oy_ + 20, t);
    // seg17:0x3ba6(0): Arial 13 x 6 in 0x00FFFF00 from DGROUP:0x5002 + 0xa2, 18 px apart. The
    // choice among 2051-2053 and the allies/enemies bytes are M3: the port draws 2053, as
    // start-0001 shows for wave 1 [OPEN: seg17:0x3ba6].
    c_.text(Font{13, 6, 0}, 0x00FFFF00, kTaLeft, x, pages_oy_ + 0xa2, h_.lang(2053));
}

void Briefing::icon_label(int k, int x, int y) {
    // seg17:0xc2e(k, x, y): one line, CreateFont(12, 0, ...) "Arial" in 0x0200FF80,
    // TA_CENTER, at (x + 32, y + 68) [BYTES seg17:0xc3b-0xccc].
    if (k < 0 || k >= static_cast<int>(setup_.names.size())) return;
    std::string name = setup_.names[k];
    if (name.size() > 35) name.resize(35);  // the 35-byte record
    c_.text(Font{12, 0, 0}, 0x0200FF80, kTaCenter, x + 32, y + 68, name);
}

int Briefing::routine(shell::GameState& s, int p, int init) {
    // The 19-entry table seg17:0x355 (init 0) and the 17-entry table seg17:0x5d1 (init 1)
    // with DGROUP:0x9460 = 0. A failed condition returns 1 from the frame table (the phase
    // passes in the same frame) and nothing from the init table [BYTES seg17:0x1d5-0x379
    // seg17:0x3b2 seg17:0x5d1]. In state 3 the wave is at least 1, so phases 3 and 4 always
    // skip [BYTES seg9:0x5fbf].
    const WaveFlags& f = setup_.flags;
    const int skip = init ? 0 : 1;
    cur_phase_ = p;
    gs_ = &s;
    switch (p) {
    case 1: return phase1(s, init);
    case 2:
        if (f.f94e == 0) return skip;
        return race(init);
    case 3:
        if (wave_ != 0) return skip;
        return held(init, "briefing phase 3", "seg17:0xd22");
    case 4:
        if (wave_ != 0 || f.f92f2 == 0) return skip;
        return slide_in(init, 3);
    case 5:
        if (!(f.f94e == 0 && wave_ == 1)) return skip;
        return slide_in(init, 7);
    case 6:
        if (f.f92fe == 0) return skip;
        return slide_in(init, 9);
    case 7:
        if (wave_ != 1 || f.f92f0 == 0) return skip;
        return defenders(init);
    case 8:
        if (f.f4ee0 == 0) return skip;
        if (init) h_.todo("briefing phase 8 ship lookup and page seg17:0x7614", "seg17:0x4b9");
        return slide_in(init, 8);
    case 9:
        if (f.f4ee2 == 0) return skip;
        if (init) h_.todo("briefing phase 9 ship lookup and page seg17:0x7614", "seg17:0x4b9");
        return slide_in(init, 8);
    case 11: return ship(init);
    default:  // 10 and 12-17: the help window's pages only
        return skip;
    }
}

void Briefing::load_page(int phase, int kind, Tick hold) {
    // seg17:0x30b0(kind, hold): DGROUP:0x9214 (the meal) and DGROUP:0x9468 are not kept by
    // the port yet: 0.
    const int id = page_resource(kind, 0, 0);
    std::string text;
    if (id == 0) {
        h_.todo("generated briefing page kind " + std::to_string(kind), "seg17:0x30b0");
    } else {
        text = cut_at_nul(h_.page(id), 2000);  // the zero-filled 2000-byte block DGROUP:0x6078
    }
    pages_.set_page(std::move(text), page_wraps(phase), hold);
    pages_.set_origin(pages_ox_, pages_oy_, box_.pr);
    pages_.step(now_, 1, 0, phase);
}

int Briefing::phase1(shell::GameState& s, int init) {
    // seg17:0x600 [BYTES seg17:0x600-0xb71].
    const int b = setup_.size_level;
    const int o = (6 << b) >> 1;
    const int sz = (32 << b) >> 1;
    const int slot = this->slot();
    const int total = setup_.icons;  // DGROUP:0x91fe
    auto gone = [&](int k) {
        return k >= 0 && k < static_cast<int>(setup_.gone.size()) && setup_.gone[k];
    };
    const int L = box_.pl, R = box_.pr, T = box_.pt, Yb = box_.pb;
    if (init) {
        if (icons_left_ == 0) {
            t187a_ = now_ - 1;  // seg17:0x63f: the next frame ends the phase
            return 1;
        }
        t187a_ = 0;
        i1882_ = 0;
        i1880_ = 0;
        i187e_ = 0;
        if (gone(0)) {
            do {
                ++i1880_;
                if (!gone(i1880_)) break;
            } while (i1880_ < total);
        }
    }
    if (t187a_ != 0) return t187a_ < now_ ? 1 : 0;
    const int sub = s.briefing_skip();  // DGROUP:0x9462 is phase 1's sub-step
    if (sub == 0) {
        // Grow: one icon at a time, 4 to 64 px, centred at the page middle + 52.
        // (The "NO ICONS LEFT" branch for DGROUP:0x91fc = 0 runs only with the help window.)
        const int size = i1882_ + 4;
        const int x = ((-L - (slot * icons_left_ - R)) / 2 - size / 2) + i187e_ * slot + L + 32;
        const int y = ((T - Yb) / 2 - size / 2) + Yb + 52;
        c_.icon(x, y, size, size, (sz + o) * i1880_ + o, o, sz, sz);
        i1882_ += 3;
        if (i1882_ == 12) h_.sound_at(0x13, x, y, 4);  // seg10:0x754a(0x13, x, y, 4) (M7)
        if (i1882_ > 60) {
            icon_label(i1880_, x, y);  // on the frame that draws size 64 [BYTES seg17:0x880]
            do {
                ++i1880_;
                if (total <= i1880_) break;
            } while (gone(i1880_));
            ++i187e_;
            i1882_ = 0;
        }
        if (total <= i1880_) {
            i1882_ = 0;
            s.set_briefing_skip(sub + 1);
            pages_ox_ = (R - L) / 2 + L - 265;
            pages_oy_ = y - 106;
            load_page(s.briefing_phase(), s.briefing_phase(), wave_ == 0 ? 5000 : 3000);
        }
    } else if (sub == 1) {
        const int r = pages_.step(now_, 0, 0, s.briefing_phase());
        if (r == 1) {
            // seg9:0x6624 with the wave incremented (M3).
        } else if (r == 2) {
            i1880_ = 0;
            s.set_briefing_skip(sub + 1);
            i1884_ = 60;
            i1882_ = 60;
            h_.sound(0x13);  // seg10:0x754a [OPEN: its arguments]; played as seg10:0x744c (M7)
        }
    } else if (sub == 2) {
        // Shrink: by 4 px per frame, x from the previous size, y from the current one.
        const int base = (-L - (slot * total - R)) / 2;
        c_.icon((base - (i1884_ + 4) / 2) + i1880_ * slot + L + 32,
                ((T - Yb) / 2 - (i1882_ + 4) / 2) + Yb + 52, i1884_ + 4, i1884_ + 4,
                (sz + o) * i1880_ + o - 2, o - 2, sz + 4, sz + 4);
        const int next = i1882_ - 4;
        i1884_ = i1882_;
        i1882_ = next;
        if (next == 48) {
            h_.sound(0x6e);
            h_.sound(i1880_ == 0 ? 0x15 : 0x13);
        }
        if (i1882_ < 1) {
            c_.black(i1880_ * slot + base + L - 6, (T - Yb) / 2 + Yb + 20, 76, slot);
            i1882_ = 60;
            i1884_ = 60;
            ++i1880_;
            if (total <= i1880_) t187a_ = now_ + 1000;
        }
    }
    return 0;
}

}  // namespace is::briefing
