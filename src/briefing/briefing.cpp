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

int Typewriter::step(Tick now, int reset, int instant) {
    // DGROUP:0x9460 (the help window) is 0 in state 3: the briefing frame does not run while it
    // is open.
    if (reset) {  // seg9:0x8d26
        x_ = left_ + 25;
        y_ = top_ + 55;
        if (!instant) h_.todo("CRT flash seg15:0x84b0 at the briefing text", "seg15:0x84b0");
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
        if (!instant) h_.sound(0x2b);  // seg10:0x754a(0x2b, DGROUP:0x1834, DGROUP:0x1836, 4)
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
        if (!instant) h_.todo("CRT flash seg15:0x84b0 at a briefing page", "seg15:0x84b0");
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
    case 2: case 8: case 9: case 11: case 12: case 16: case 406: return 0;
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
        h_.todo("briefing ship page seg17:0x20fa(1, 0, 1)", "seg17:0x20fa");
    }
    if (s.briefing_skip() == -2) advance(s);
    if (s.briefing_skip() == -1) {
        if (s.briefing_phase() == 11) {  // seg17:0x93-0x10b
            black_page();
            h_.sound(0x6e);
            h_.todo("briefing ship page seg17:0x20fa(1, 1, 0)", "seg17:0x20fa");
            // [DESIGN] The page's result is open; the port answers 1, so Space leaves the
            // ship page as at any other phase.
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
            h_.todo("seg17:0x6036 at the briefing's end", "seg17:0x6036");
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
    // A phase whose drawing and duration are open: the port logs it at its init and advances
    // at the next frame [DESIGN].
    if (init) {
        h_.todo(what, address);
        return 0;
    }
    return 1;
}

int Briefing::slide_in(int init, int kind) {
    // seg17:0x1b3e: the object slide-in with its frame count (shell.md "Open questions"). Its
    // drawing is open: the port holds the phase for that many frames on a black page
    // [DESIGN].
    if (init) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "briefing object slide-in kind %d", kind);
        h_.todo(buf, "seg17:0x1b3e");
        frames_left_ = slide_in_frames(kind);
        return 0;
    }
    return --frames_left_ <= 0 ? 1 : 0;
}

int Briefing::routine(shell::GameState& s, int p, int init) {
    // The 19-entry table seg17:0x355 (init 0) and the 17-entry table seg17:0x5d1 (init 1)
    // with DGROUP:0x9460 = 0. A failed condition returns 1 from the frame table (the phase
    // passes in the same frame) and nothing from the init table [BYTES seg17:0x1d5-0x379
    // seg17:0x3b2 seg17:0x5d1].
    const WaveFlags& f = setup_.flags;
    const int skip = init ? 0 : 1;
    switch (p) {
    case 1: return phase1(s, init);
    case 2:
        if (f.f94e == 0) return skip;
        return held(init, "briefing phase 2", "seg17:0x13ca");
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
        return held(init, "briefing phase 7", "seg17:0x193c");
    case 8:
        if (f.f4ee0 == 0) return skip;
        if (init) h_.todo("briefing phase 8 ship lookup", "seg17:0x3b2");
        return slide_in(init, 8);
    case 9:
        if (f.f4ee2 == 0) return skip;
        if (init) h_.todo("briefing phase 9 ship lookup", "seg17:0x3b2");
        return slide_in(init, 8);
    case 11:
        // seg17:0x20fa(init, 0, 0): the ship page of DGROUP:0x9464 [OPEN].
        return held(init, "briefing ship page seg17:0x20fa", "seg17:0x20fa");
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
        if (i1882_ == 12) h_.sound(0x13);  // seg10:0x754a(0x13, x, y, 4)
        if (i1882_ > 60) {
            h_.todo("briefing icon label", "seg17:0xc2e");
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
            h_.sound(0x13);  // seg10:0x754a [OPEN: its arguments]
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
