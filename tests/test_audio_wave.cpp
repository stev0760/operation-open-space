// INSPACE's sound wrappers in a wave (src/audio/engine.h, wave_view.h; docs/re/sound.md
// "INSPACE wrappers" steps 4 and 5): seg10:0x7896's q on the source point before seg8:0x29ea,
// seg8:0x285a's cell distance from the player's world point, the activity-rectangle test and
// the Size wrap around the player's screen point (M4 s8 sound track). Portable.
#include <cstdint>
#include <memory>
#include <string>

#include "audio/engine.h"
#include "audio/mixer.h"
#include "doctest.h"
#include "world/world.h"

namespace a = is::audio;

namespace {

struct Source : a::WaveSource {
    bool has_module(int) override { return true; }
    a::PcmPtr slot(int, int resource) override {
        if (resource == 112 || resource == 142 || resource == 165 || resource == 181) return nullptr;
        return std::make_shared<a::Pcm>(std::size_t{100}, std::uint8_t{200});
    }
    a::PcmPtr file(const std::string&) override { return nullptr; }
};

a::Settings settings(int sound_system, int channels) {
    a::Settings s{};
    s.sound_fx = 1;
    s.voice_fx = 1;
    s.music = 1;
    s.sound_volume = 100;
    s.fx_music = 50;
    s.stereo_effect = 2;
    s.purge_sound = 1;
    s.sound_system = sound_system;
    s.mix_channels = channels;
    return s;
}

// A wave: the player's object at world (8000, 8000), on screen at (320, 200); one activity
// rectangle, the view grown by 600 (seg8:0x0) around it; pool 6 record m at `obj`.
struct WaveRig {
    a::Mixer mixer;
    Source src;
    std::uint32_t clock = 10000;
    a::Engine eng{mixer, src, [this] { return clock; }};
    a::WaveView view;
    a::Point obj{8000, 8000};
    bool wave = true;
    explicit WaveRig(int sound_system = 1, int channels = 1) {
        eng.command0(settings(sound_system, channels), 1, 3200, 3200);
        view.x = 8000;
        view.y = 8000;
        view.scr_x = 320;
        view.scr_y = 200;
        view.size = 1;
        view.rect_count = 1;
        const int r[4] = {8000 - 2560 - 600, 8000 - 1600 - 600, 8000 + 2560 + 600, 8000 + 1600 + 600};
        for (int i = 0; i < 4; ++i) view.rects[0][i] = r[i];
        auto& w = eng.world();
        w.pool_count = 14;
        w.source = [this](int mode, int, int, a::Point& p) {
            if (mode != 1) return false;
            p = obj;
            return true;
        };
        // seg8:0x29ea at Size 1: one pixel is 8 world units, the player at (320, 200).
        w.to_view = [this](a::Point p) {
            return a::Point{(p.x - view.x) / 8 + view.scr_x, (p.y - view.y) / 8 + view.scr_y};
        };
        w.view = [this](a::WaveView& v) {
            if (!wave) return false;
            v = view;
            return true;
        };
    }
    int play(int id, std::uint32_t now) { return eng.wrap_754a(id, 6, 0, 1, now); }
};

}  // namespace

TEST_CASE("audio: seg8:0x285a's cell distance") {
    // core.md's fuse vectors (s5-weapons.md 9.4): the fuse point (8022, 8022) against the
    // missile's (x + extent / 2, 8172).
    CHECK(a::cell_distance({8022, 8022}, {7772, 8172}) == 36);
    CHECK(a::cell_distance({8022, 8022}, {8476, 8172}) == 60);
    CHECK(a::cell_distance({8022, 8022}, {8412, 8172}) == 52);
    // One point: e = 0 gives 1 [BYTES seg8:0x29db].
    CHECK(a::cell_distance({100, 100}, {100, 100}) == 1);
    // The wrap: cells 12 and 3187 are 25 apart across the 3200 seam.
    CHECK(a::cell_distance({100, 100}, {25500, 100}) == 25);
    CHECK(a::cell_distance({25500, 100}, {100, 100}) == 25);
    // The preload point's x, -11111 sar 3 = -1389: from cell 1000 that is 2389, moved to -811.
    CHECK(a::cell_distance({8000, 0}, {-11111, 0}) == 811);
    // The same routine as world::distance_cells (isaudio cannot link isworld).
    auto mix = [](unsigned i, unsigned k) { return static_cast<int>((i * k) % 25600u); };
    for (unsigned i = 0; i < 400; ++i) {
        const int x1 = mix(i, 7919u), y1 = mix(i, 104729u);
        const int x2 = mix(i, 15485863u), y2 = mix(i, 32452843u);
        std::int16_t dx = 0, dy = 0;
        const int w = is::world::distance_cells(static_cast<std::int16_t>(x1), static_cast<std::int16_t>(y1),
                                                static_cast<std::int16_t>(x2), static_cast<std::int16_t>(y2),
                                                dx, dy);
        CHECK(a::cell_distance({x1, y1}, {x2, y2}) == w);
    }
}

TEST_CASE("audio: seg10:0x7896 measures the source point from the player's world point") {
    WaveRig r;
    // 250 cells to the right, inside the activity rectangle: q = 1.
    r.obj = {8000 + 2000, 8000};
    CHECK(r.play(2, 1000) == 1);
    CHECK(r.eng.record(0).id == 2);
    CHECK(r.eng.record(0).q == 1);
    // 341 ... 800 cells: q = 0, refused by admission rule 1 while the record holds q.
    r.clock += 100;
    r.obj = {8000 + 8 * 400, 8000};
    CHECK(r.play(2, 1100) == 0);
    CHECK(r.eng.record(0).start == 10000);
    // Above 800 cells nothing reaches ordinal 9, and the pending follow-up is cleared.
    r.eng.wrap_754a(0x6a, 0, 0, 0, 1150);  // 0x20 now, 0x21 at 2150
    CHECK(r.eng.pending_deadline() == 2150);
    r.obj = {8000 + 8 * 801, 8000};
    CHECK(r.play(2, 1200) == 0);
    CHECK(r.eng.pending_deadline() == 0);
    CHECK(r.eng.pending_id() == 0);
}

TEST_CASE("audio: seg10:0x7896 needs an activity rectangle for q = 1") {
    WaveRig r;
    // 300 cells straight down: within 340, but below the rectangle's bottom (8000 + 2200).
    r.obj = {8000, 8000 + 8 * 300};
    r.play(2, 1000);
    CHECK(r.eng.record(0).id == 2);
    CHECK(r.eng.record(0).q == 0);
    // In a second rectangle (the wrap's split) it is 1 again.
    r.view.rect_count = 2;
    const int second[4] = {7000, 9000, 9000, 11000};
    for (int i = 0; i < 4; ++i) r.view.rects[1][i] = second[i];
    r.eng.wrap_754a(0x6e, 0, 0, 0, 1050);  // stops all
    r.clock += 100;
    r.play(2, 1100);
    CHECK(r.eng.record(0).q == 1);
    // A rectangle past DGROUP:0x9158's count does not count.
    r.view.rect_count = 1;
    r.eng.wrap_754a(0x6e, 0, 0, 0, 1150);
    r.clock += 100;
    r.play(2, 1200);
    CHECK(r.eng.record(0).q == 0);
}

TEST_CASE("audio: q comes before seg8:0x29ea and the Size wrap") {
    WaveRig r;
    // A screen transform that throws every point far away changes nothing in q.
    r.eng.world().to_view = [](a::Point p) { return a::Point{p.x + 30000, p.y}; };
    r.obj = {8000 + 800, 8000};
    r.play(2, 1000);
    CHECK(r.eng.record(0).q == 1);
}

TEST_CASE("audio: the thrown weapon's spin wraps against core-0009 (MixChannels 1)") {
    WaveRig r;  // STANDARD SOUND, one channel, as core-0009
    // seg11:0x89e4's 0x69, then the launch sound at the ship: q = 1, the priority.
    r.eng.wrap_744c(0x69, 1000);
    r.obj = {8000 + 40, 8000};
    CHECK(r.play(2, 1000) == 1);
    // The spin wraps at 16 passes: 8 cells a pass relative to the ship (4·16 world units).
    const int cells[] = {128, 256, 384};
    const int expect[] = {1, 1, 0};  // the third, at 384 cells, is q = 0 and refused
    std::uint32_t now = 1000;
    for (int k = 0; k < 3; ++k) {
        now += 430;
        r.clock += 430;
        r.obj = {8000 + 8 * cells[k], 8000};
        const std::uint32_t before = r.eng.record(0).start;
        CHECK(r.play(2, now) == expect[k]);
        CHECK((r.eng.record(0).start != before) == (expect[k] == 1));
    }
}

TEST_CASE("audio: the Size wrap moves the panned point around the player's screen point") {
    // DYNAMIC SOUND with four channels, so every priority play sets the pan through ordinal 8.
    auto pan = [](int size, int dx_cells) {
        WaveRig r(3, 4);
        r.view.size = size;
        r.obj = {8000 + 8 * dx_cells, 8000};
        r.play(2, 1000);
        return r.mixer.volume();
    };
    auto plain = [](int screen_dx) {
        WaveRig r(3, 4);
        r.eng.world().source = nullptr;
        r.eng.ordinal8(640, 480, 1, 1, {320 + screen_dx, 200});
        return r.mixer.volume();
    };
    // Size 1: no wrap, the point 200 px right of the player.
    CHECK(pan(1, 200) == plain(200));
    // Size 2: scr + (2·200) sar 2 = scr + 100.
    CHECK(pan(2, 200) == plain(100));
    // Size 0: scr + 2·200.
    CHECK(pan(0, 200) == plain(400));
    // Outside a wave nothing wraps.
    WaveRig off(3, 4);
    off.view.size = 2;
    off.wave = false;
    off.obj = {8000 + 8 * 200, 8000};
    off.play(2, 1000);
    CHECK(off.mixer.volume() == plain(200));
}

TEST_CASE("audio: a mode-5 preload follows seg10:0x7896") {
    // Outside a wave every point is near: the deferred slot 2 loads.
    {
        WaveRig r;
        r.wave = false;
        CHECK_FALSE(r.eng.loaded(2));
        r.eng.wrap_754a(2, 0, 0, 5, 1000);
        CHECK(r.eng.loaded(2));
    }
    // In a wave the point (-11111, -11111) (case 5's AX in both words, seg10:0x775c-0x7762
    // seg10:0x774e) is 811 cells on each axis from a player at (8000, 8000): q = -1, nothing
    // reaches ordinal 9 and the slot stays unloaded.
    {
        WaveRig r;
        r.eng.wrap_754a(2, 0, 0, 5, 1000);
        CHECK_FALSE(r.eng.loaded(2));
    }
    // A player near the preload point's cell lets it through (q = 0 still loads: the preload
    // returns at p.x = -11111 before the admission).
    {
        WaveRig r;
        r.view.x = 14600;  // cell 1825, -1389 + 3200 = 1811: 14 cells
        r.view.y = 14600;  // the same on y: the point's y is -11111 too, not b
        r.eng.wrap_754a(2, 0, 0, 5, 1000);
        CHECK(r.eng.loaded(2));
    }
    // b is not the point's y: a player near (-11111, b) but far from y -11111 keeps the slot
    // unloaded (cell 0 against 1811: 1389 cells on y).
    {
        WaveRig r;
        r.view.x = 14600;
        r.view.y = 0;
        r.eng.wrap_754a(2, 0, 0, 5, 1000);
        CHECK_FALSE(r.eng.loaded(2));
    }
}
