// Recording fakes for the intro's Canvas and Host (src/intro/intro.h): a fake clock, fixed
// strings and a canvas that logs every call, so the tests can follow the phases frame by frame.
#pragma once

#include <cstdarg>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "intro/intro.h"

namespace intro_test {

using is::intro::Colour;
using is::intro::FontId;
using is::intro::Image;
using is::intro::Point;
using is::intro::Tick;

inline const char* image_name(Image i) {
    switch (i) {
    case Image::kStrip: return "strip";
    case Image::kSdLogo: return "sdlogo";
    case Image::kCredits: return "credits";
    case Image::kTitle: return "title";
    case Image::kTitleReveal: return "reveal";
    }
    return "?";
}

class FakeCanvas : public is::intro::Canvas {
public:
    std::vector<std::string> ops;
    bool record = true;

    void put(const char* fmt, ...) {
        if (!record) return;
        char buf[256];
        va_list a;
        va_start(a, fmt);
        std::vsnprintf(buf, sizeof buf, fmt, a);
        va_end(a);
        ops.emplace_back(buf);
    }
    int count_prefix(const std::string& p) const {
        int n = 0;
        for (const auto& s : ops)
            if (s.compare(0, p.size(), p) == 0) ++n;
        return n;
    }
    std::vector<std::string> with_prefix(const std::string& p) const {
        std::vector<std::string> r;
        for (const auto& s : ops)
            if (s.compare(0, p.size(), p) == 0) r.push_back(s);
        return r;
    }

    void build_strip() override { put("build_strip"); }
    void load_sd_logo() override { put("load_sd_logo"); }
    void build_credit_cells(int mode) override { put("build_credit_cells %d", mode); }
    void load_title_picture() override { put("load_title_picture"); }
    void make_title_reveal() override { put("make_title_reveal"); }
    void free_image(Image i) override { put("free %s", image_name(i)); }
    void black(int x, int y, int w, int h) override { put("black %d %d %d %d", x, y, w, h); }
    void blit(Image s, int dx, int dy, int w, int h, int sx, int sy) override {
        put("blit %s %d %d %d %d %d %d", image_name(s), dx, dy, w, h, sx, sy);
    }
    void stretch(Image s, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh) override {
        put("stretch %s %d %d %d %d %d %d %d %d", image_name(s), dx, dy, dw, dh, sx, sy, sw, sh);
    }
    void double_copy(Image s, int dx, int dy, int sx, int sy, int sw, int sh) override {
        put("double %s %d %d %d %d %d %d", image_name(s), dx, dy, sx, sy, sw, sh);
    }
    void merge(Image s, int dx, int dy, int w, int h, int sx, int sy, Colour b) override {
        put("merge %s %d %d %d %d %d %d %08x", image_name(s), dx, dy, w, h, sx, sy, b);
    }
    void line(Colour pen, int width, int x0, int y0, int x1, int y1) override {
        put("line %08x %d %d %d %d %d", pen, width, x0, y0, x1, y1);
    }
    FontId create_font(const is::intro::FontSpec& f) override {
        fonts[++next_font] = f;
        put("font %d %d %d %d %s", f.height, f.width, f.weight, f.italic ? 1 : 0, f.face);
        return next_font;
    }
    FontId select_font(FontId f) override {
        const FontId old = font;
        font = f;
        return old;
    }
    void delete_font(FontId f) override { fonts.erase(f); }
    Colour text_colour(Colour c) override {
        const Colour o = text;
        text = c;
        return o;
    }
    Colour bk_colour(Colour c) override {
        const Colour o = bk;
        bk = c;
        return o;
    }
    void bk_mode(int m) override { mode = m; }
    void text_align(unsigned a) override { align = a; }
    void text_out(int x, int y, const std::string& s) override {
        if (align & 1) {  // TA_UPDATECP: drawn at the current position, which advances
            put("text %08x cp %d %d %s", text, cp.x, cp.y, s.c_str());
            cp.x += text_width(s);
            return;
        }
        put("text %08x %d %d %u %d %s", text, x, y, align, mode, s.c_str());
    }
    void move_to(int x, int y) override { cp = {x, y}; }
    Point current_position() override { return cp; }
    // Each byte is as wide as the selected font's width, or half its height.
    int text_width(const std::string& s) override {
        const auto it = fonts.find(font);
        const int w = it == fonts.end() ? 8 : it->second.width ? it->second.width : it->second.height / 2;
        return w * static_cast<int>(s.size());
    }
    void font_text(int f, int a, Colour c, int x, int y, const std::string& s) override {
        put("font_text %d %d %08x %d %d %s", f, a, c, x, y, s.c_str());
    }
    void palette_scheme(int c, int m) override { put("scheme %d %d", c, m); }
    void palette_ramp(int c) override { put("ramp %d", c); }
    void palette_animate() override { put("animate"); }
    void release_dc() override {
        text = 0;
        bk = 0xffffff;
        mode = 2;
        align = 0;
        cp = {0, 0};
        font = 0;
    }

    std::map<FontId, is::intro::FontSpec> fonts;
    FontId next_font = 0, font = 0;
    Colour text = 0, bk = 0xffffff;
    int mode = 2;
    unsigned align = 0;
    Point cp;
};

class FakeHost : public is::intro::Host {
public:
    Tick t = 1000;
    is::intro::Area a{1024, 768, 683};
    std::vector<int> sounds, music_codes;
    std::vector<std::string> todos;
    int shown = 0, demo_starts = 0;
    bool started = false, done = false;
    Tick start = 0;
    int calibrate = 14000, mix = 1;
    std::map<int, std::string> strings;
    std::map<int, std::string> exe;
    std::vector<std::uint8_t> page_res;

    FakeHost() {
        strings = {{2118, "presents.. "},
                   {2119, "press ESC to play Inner Space.. "},
                   {2120, "Basic Key Commands"},
                   {2121, "OPERATION:"},
                   {2310, " Press 'R' to order the full game package "},
                   {2337, "Inner Space"},
                   {2338, "Limited Inner Space"},
                   {2339, " Standard Edition"},
                   {2340, " Plus Edition"},
                   {2341, "You are on day %d of your demo trial period."},
                   {2342, "You are on day %d of your 35 day demo registration."},
                   {2343, "The game isn't valid on this hard drive."},
                   {2542, "ESC to play    -    Alt-X to Exit"},
                   {1000, "Directed By"},
                   {1001, "Programming By"},
                   {1002, "Music Composed By"},
                   {1003, "Voice Effects"},
                   {1004, "Thanks to"},
                   {1005, "For"},
                   {980, "Bill Stewart"},
                   {981, "Bill Stewart & SDi Staff"},
                   {982, "Doug 'The Thin Man' Blackley "},
                   {983, "Nancy Stewart & James Stewart"},
                   {984, "Our Testers!"},
                   {985, "Chaela & Chance"}};
        for (int i = 0; i < 11; ++i) {
            strings[1223 + i] = "key" + std::to_string(i);
            strings[1323 + i] = "desc" + std::to_string(i);
        }
        strings[1207] = "F1, F2";
        strings[1307] = "Online Help";
        strings[1209] = "Alt G";
        strings[1309] = "Game Settings";
        strings[1213] = "Alt X";
        strings[1313] = "Exit Game";
        exe = {{167, "129"}, {233, "08"}, {607, "Aug 9/2000"}};
        std::string page;
        for (int i = 0; i < 21; ++i) page += "line " + std::to_string(i) + "\r\n";
        page_res.assign(page.begin(), page.end());
        page_res.push_back(0);
    }

    Tick now() override { return t; }
    is::intro::Area area() override { return a; }
    void sound(int id) override { sounds.push_back(id); }
    void music(int c) override { music_codes.push_back(c); }
    void todo(const std::string& w, const char*) override { todos.push_back(w); }
    void show_window() override { ++shown; }
    void calibrate_start(Tick n) override {
        if (done) return;
        started = true;
        start = n;
    }
    bool calibrating() override { return started && !done; }
    bool calibrated() override { return done; }
    bool calibrate_step(Tick n) override {
        if (done || n - start <= 1000) return false;
        done = true;
        return true;
    }
    int calibrate_value() override { return calibrate; }
    int mix_channels() override { return mix; }
    void present_wait(Tick ms) override { t += ms; }
    void demo_start() override { ++demo_starts; }
    is::intro::LocalDay local_day() override { return {126, 297}; }  // 2026-10-25
    std::string lang(int id) override {
        const auto it = strings.find(id);
        return it == strings.end() ? std::string() : it->second;
    }
    std::string exe_string(int id) override {
        const auto it = exe.find(id);
        return it == exe.end() ? std::string() : it->second;
    }
    std::vector<std::uint8_t> lang_resource(int type, int id) override {
        return type == 5000 && id == 136 ? page_res : std::vector<std::uint8_t>{};
    }
    bool has_todo(const std::string& part) const {
        for (const auto& s : todos)
            if (s.find(part) != std::string::npos) return true;
        return false;
    }
};

// Runs frames 13 ms apart (ADR 0001 decision 8) until `pred` holds or `max` frames ran;
// returns the number of frames.
template <class Pred>
int run_until(is::intro::Intro& in, FakeHost& h, Pred pred, int max = 100000) {
    int n = 0;
    while (!pred() && n < max) {
        h.t += 13;
        in.frame(h.t, false);
        ++n;
    }
    return n;
}

}  // namespace intro_test
