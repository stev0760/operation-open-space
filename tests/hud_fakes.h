// A recording hud::Canvas and a configurable hud::Host for the HUD's host tests.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "hud/canvas.h"
#include "hud/host.h"

namespace hudtest {

using namespace is::hud;

struct Call {
    enum Op { kPat, kBlt, kLines, kRect, kPixel, kFrame, kFont, kText, kView, kNoView, kCreate, kStretch };
    Op op = kPat;
    Img dst = Img::kScreen, src = Img::kScreen;
    int x = 0, y = 0, w = 0, h = 0, sx = 0, sy = 0, sw = 0, sh = 0;
    std::uint32_t rop = 0;
    Brush brush;
    Pen pen;
    std::vector<Point> pts;
    int style = 0;    // frame style, font, text align
    int align = 0;
    Colour colour = 0;
    std::string text;
};

class RecordingCanvas : public Canvas {
public:
    std::vector<Call> calls;
    std::map<Img, std::pair<int, int>> sizes;

    bool has(Img img) const override { return img == Img::kScreen || sizes.count(img) != 0; }
    int width(Img img) const override {
        auto it = sizes.find(img);
        return it == sizes.end() ? 0 : it->second.first;
    }
    int height(Img img) const override {
        auto it = sizes.find(img);
        return it == sizes.end() ? 0 : it->second.second;
    }
    void create(Img img, int w, int h) override {
        sizes[img] = {w, h};
        Call c;
        c.op = Call::kCreate, c.dst = img, c.w = w, c.h = h;
        calls.push_back(c);
    }
    void pat_blt(Img dst, int x, int y, int w, int h, std::uint32_t rop,
                 const Brush& brush) override {
        Call c;
        c.op = Call::kPat, c.dst = dst, c.x = x, c.y = y, c.w = w, c.h = h, c.rop = rop;
        c.brush = brush;
        calls.push_back(c);
    }
    void bit_blt(Img dst, int x, int y, int w, int h, Img src, int sx, int sy, std::uint32_t rop,
                 const Brush& brush) override {
        Call c;
        c.op = Call::kBlt, c.dst = dst, c.x = x, c.y = y, c.w = w, c.h = h, c.src = src;
        c.sx = sx, c.sy = sy, c.rop = rop, c.brush = brush;
        calls.push_back(c);
    }
    void lines(Img dst, const Pen& pen, const Point* p, int n) override {
        Call c;
        c.op = Call::kLines, c.dst = dst, c.pen = pen;
        c.pts.assign(p, p + n);
        calls.push_back(c);
    }
    void rectangle(Img dst, const Pen& pen, const Brush& brush, int l, int t, int r,
                   int b) override {
        Call c;
        c.op = Call::kRect, c.dst = dst, c.pen = pen, c.brush = brush;
        c.x = l, c.y = t, c.w = r, c.h = b;
        calls.push_back(c);
    }
    void set_pixel(Img dst, int x, int y, Colour col) override {
        Call c;
        c.op = Call::kPixel, c.dst = dst, c.x = x, c.y = y, c.colour = col;
        calls.push_back(c);
    }
    void stretch_blt(Img dst, int x, int y, int w, int h, Img src, int sx, int sy, int sw, int sh,
                     std::uint32_t rop) override {
        Call c;
        c.op = Call::kStretch, c.dst = dst, c.x = x, c.y = y, c.w = w, c.h = h, c.src = src;
        c.sx = sx, c.sy = sy, c.sw = sw, c.sh = sh, c.rop = rop;
        calls.push_back(c);
    }
    void frame(Img dst, int x, int y, int w, int h, int style) override {
        Call c;
        c.op = Call::kFrame, c.dst = dst, c.x = x, c.y = y, c.w = w, c.h = h, c.style = style;
        calls.push_back(c);
    }
    void font_text(int font, int align, Colour col, int x, int y,
                   const std::string& text) override {
        Call c;
        c.op = Call::kFont, c.style = font, c.align = align, c.colour = col, c.x = x, c.y = y;
        c.text = text;
        calls.push_back(c);
    }
    void text_out(Img dst, int x, int y, unsigned align, Colour col,
                  const std::string& text) override {
        Call c;
        c.op = Call::kText, c.dst = dst, c.x = x, c.y = y, c.align = static_cast<int>(align);
        c.colour = col, c.text = text;
        calls.push_back(c);
    }
    void set_view(int x, int y, int w, int h) override {
        Call c;
        c.op = Call::kView, c.x = x, c.y = y, c.w = w, c.h = h;
        calls.push_back(c);
    }
    void clear_view() override {
        Call c;
        c.op = Call::kNoView;
        calls.push_back(c);
    }

    std::vector<Call> of(Call::Op op) const {
        std::vector<Call> out;
        for (const Call& c : calls)
            if (c.op == op) out.push_back(c);
        return out;
    }
};

class FakeHost : public Host {
public:
    int state_ = 1;
    is::shell::PlayArea area_{1024, 768, 683, 683};
    bool border_ = true;
    std::uint32_t now_ = 1000;
    bool race_ = false, demo_ = false, alt_ = false, blink_ = false, mono_ = false;
    int player_ = 0, watch_ = -1, view_mode_ = 0;
    std::uint32_t score_ = 0, ticks_ = 0;
    ShipView ship_;
    WeaponView weapon_;
    int fr_set_ = 36, fr_actual_ = 35;
    std::vector<std::pair<int, int>> messages;
    mutable std::vector<std::string> trace;  // the dispatch and other calls, in order
    int bad_index = 0;
    std::map<int, int> in_use_;
    std::map<std::pair<int, int>, bool> not_ok;
    std::map<std::pair<int, int>, int> dist_;
    std::vector<std::vector<int>> targets;  // set_target(ship, pool, rec)
    int target_pool_ = -1, target_rec_ = 0;
    Point point_{}, player_point_{};
    int dir16_ = 0, attitude_ = 0;

    int state() const override { return state_; }
    is::shell::PlayArea area() const override { return area_; }
    bool border() const override { return border_; }
    std::uint32_t now() const override { return now_; }
    std::string lang(int id) override {
        switch (id) {
        case 2063: return "SCORE";
        case 930: return "CURRENT ";
        case 931: return "TARGET  (";
        case 932: return "AUTO";
        case 933: return "MANUAL";
        case 929: return "MESSAGE from";
        case 928: return "GAME STATUS";
        case 927: return " frames/ second";
        case 2191: return "FULL OUT";
        case 2192: return " SET";
        case 2193: return " ACTUAL";
        case 934: return "WEAPON SYSTEM";
        case 935: return "FUEL - ";
        default: return "#" + std::to_string(id);
        }
    }
    bool monochrome() const override { return mono_; }
    void seed_rng() override { trace.push_back("srand"); }
    bool race() const override { return race_; }
    bool demo_gate() const override { return demo_; }
    bool alt_view() const override { return alt_; }
    int player() const override { return player_; }
    int watch_ship() const override { return watch_; }
    int view_mode() const override { return view_mode_; }
    bool blink() const override { return blink_; }
    std::uint32_t score() const override { return score_; }
    std::uint32_t wave_ticks() const override { return ticks_; }
    ShipView ship(int) const override { return ship_; }
    WeaponView weapon() const override { return weapon_; }
    void message(int s, int id) override { messages.push_back({s, id}); }
    int frame_rate_set() const override { return fr_set_; }
    int frame_rate_actual() const override { return fr_actual_; }
    int attitude(int, int) const override { return attitude_; }
    bool target_ok(int p, int r) const override { return !not_ok.count({p, r}) && p >= 0; }
    void bump_bad_index() override { ++bad_index; }
    std::string text_9628() const override { return "ALPHA.EXE"; }
    std::string text_8718() const override { return "Icon 0"; }
    std::string text_960a() const override { return ""; }
    std::string text_8740() const override { return "Ambulance"; }
    std::string text_8768() const override { return "GOOD ICON CAPTURE!"; }
    std::string text_87b8() const override { return "1.0 RESOURCE GAINED."; }
    // seg13:0x5a6a's record fields: an icon of extent 256 for pool 0, nothing for other pools
    // unless cells_ holds them.
    std::map<std::pair<int, int>, CellInputs> cells_;
    int game_mode_ = 0;
    bool cell(int p, int r, CellInputs& out) const override {
        trace.push_back("icon");
        auto it = cells_.find({p, r});
        if (it != cells_.end()) {
            out = it->second;
            return true;
        }
        if (p != 0) return false;
        out = CellInputs{};
        out.obj.extent = 256;
        return true;
    }
    int game_mode() const override { return game_mode_; }
    void draw_race(Canvas&) override { trace.push_back("race"); }
    int player_target_pool() const override { return target_pool_; }
    int player_target_rec() const override { return target_rec_; }
    void set_target(int s, int p, int r) override { targets.push_back({s, p, r}); }
    Point screen_point(int, int) const override { return point_; }
    Point player_point() const override { return player_point_; }
    int direction16(int, int) const override { return dir16_; }
    int in_use(int p) const override {
        auto it = in_use_.find(p);
        return it == in_use_.end() ? 0 : it->second;
    }
    int distance(int pa, int ra, int, int) const override {
        auto it = dist_.find({pa, ra});
        return it == dist_.end() ? 0x7fff : it->second;
    }
    void demo_mark() override { trace.push_back("demo_mark"); }
    void pin_player() override { trace.push_back("seg7:0x11cc"); }
    void draw_player() override { trace.push_back("seg7:0x1208"); }
    void set_58c() override { trace.push_back("58c"); }
    void draw_1f38() override { trace.push_back("seg7:0x1f38"); }
    void radar() override { trace.push_back("seg11:0x33cc"); }
};

}  // namespace hudtest
