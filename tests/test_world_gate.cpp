// The gates of wave 1 (src/world/update.h move_pool8, move_gate, gate_test; src/world/
// rock_images.h build_gate_images, create_wave_brushes; work/analysis/m3/s4-draw.md 1): the
// image slots 1 and 2 of seg10:0x3ea2, the brush tables of seg10:0x4b2e, the post draws, the
// beam's geometry and colour cycle, the moving gate, the OUT flash, the collision heat and the
// passage test seg12:0x1f7a. The wave-1 numbers are the oracle's [ORACLE wave-0001
// 06-fly-up.png]: posts at screen (523, 164) and (707, 164), a 150 x 2 beam at (557, 179).
#include <array>
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/rock_images.h"
#include "world/update.h"

using namespace is::world;

namespace {

struct Sprite {
    std::int16_t pr;
    int kind, slot, masked;
    bool operator==(const Sprite& o) const {
        return pr == o.pr && kind == o.kind && slot == o.slot && masked == o.masked;
    }
};

class GateHost : public UpdateHost {
public:
    World* w = nullptr;
    RecordingBlitter* out = nullptr;
    VisibilityGlobals vis;
    std::vector<Sprite> sprites;
    std::vector<std::array<int, 2>> passed, hazards, puffs, sounds;
    std::uint16_t hit = 0xffffu;
    int posts_hit = 0;
    World* frame_world(Pools&) override { return w; }
    Blitter* blitter() override { return out; }
    const VisibilityGlobals* visible_list() override { return &vis; }
    void draw_sprite(std::int16_t pr, int kind, int slot, int masked) override {
        sprites.push_back(Sprite{pr, kind, slot, masked});
    }
    std::uint16_t collide(std::int16_t) override { return hit; }
    void post_hit(int, std::uint16_t) override { ++posts_hit; }
    void gate_passed(int pool, int rec) override { passed.push_back({pool, rec}); }
    void hazard(int kind, int i) override { hazards.push_back({kind, i}); }
    void puff(int pool, int rec, int) override { puffs.push_back({pool, rec}); }
    void sound(int id, int pool, int rec, int) override { sounds.push_back({id, pool * 256 + rec}); }
    std::vector<int> ai;
    void gate_ai(int ship) override { ai.push_back(ship); }
};

// The wave-1 gate of the probe (tests/test_world_fixer.cpp's numbers): post 0 at (6152, 13920),
// post 1 at 6152 + 272 + 1200, the beam rectangle (6424, 14032, 7624, 14080).
struct GateRig {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    GateHost host;
    is::shell::Rng rng;
    JoyState joy;
    JoyButtons jb;
    RecordingBlitter out;
    WaveGraphics saved = wave_graphics();
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};

    GateRig() {
        host.w = &w;
        host.out = &out;
        g.now = 50000;
        g.size = 1;
        w.shift = 3;
        w.detail = 3;
        w.icons_b = 3;  // icons left: no OUT flash
        w.pools.set_in_use(kPoolGatePosts, 2);
        for (int k = 0; k < 2; ++k) {
            ObjectRecord& r = w.pools.rec(kPoolGatePosts, k);
            r.state = 0;
            r.extent = 272;
            r.size = 34;
            r.gap = 6;
            r.src_y = 6;
            r.flame = 2;
            r.mass = 0xdac;
            r.on_screen = 1;
            w.pool8[static_cast<std::size_t>(k)].at(0x1c) = 3;
        }
        post(0).x = 6152, post(0).y = 13920, post(0).scr_x = 523, post(0).scr_y = 164;
        post(1).x = 7624, post(1).y = 13920, post(1).scr_x = 707, post(1).scr_y = 164;
        GateRecord& gr = w.gate[0];
        gr.width = 1200;
        gr.w0e = 80;
        gr.post = 0;
        gr.beam = rect16(6424, 14032, 7624, 14080);
        w.beam_index = 1;  // the placement's [BYTES seg9:0x67f4 seg9:0x67fa]
        w.beam_step = 1;
        WaveGraphics& wg = wave_graphics();
        for (std::size_t n = 1; n < wg.beam.size(); ++n) wg.beam[n] = static_cast<BrushId>(0x300 + n);
        for (std::size_t n = 1; n < wg.heat.size(); ++n) wg.heat[n] = static_cast<BrushId>(0x310 + n);
        wg.gate_dc = 0x150;
        wg.out_sign = 0x151;
        wg.brushes = true;  // the ids above stand
        set_wave_blitter(nullptr);
    }
    ~GateRig() { wave_graphics() = saved; }
    ObjectRecord& post(int k) { return w.pools.rec(kPoolGatePosts, k); }
    std::vector<BlitCall> window_calls() const {
        std::vector<BlitCall> v;
        for (const BlitCall& k : out.calls)
            if (k.dst == kDcWindow) v.push_back(k);
        return v;
    }
};

BlitCall select_call(DcId dc, BrushId b) {
    BlitCall c;
    c.op = BlitCall::kSelectBrush, c.dst = dc, c.brush = b;
    return c;
}

}  // namespace

TEST_CASE("gate images: seg10:0x3ea2 builds slots 1 and 2 [BYTES seg10:0x3ea2]") {
    RecordingBlitter out;
    ImageSlotTable images{};
    WaveGraphics wg;
    build_gate_images(out, images, wg);
    // Slot 1 (DGROUP:0x39ea) colour, slot 2 (DGROUP:0x39ec) mono, both 166 x 46 [BYTES
    // seg10:0x40c6-0x40ea seg10:0x40fb seg10:0x3efc].
    REQUIRE(images[kSlotGate] != kDcNone);
    REQUIRE(images[kSlotGateMask] != kDcNone);
    CHECK(wg.gate_dc == images[kSlotGate]);
    const DcId s1 = images[kSlotGate], s2 = images[kSlotGateMask];
    int graphics = 0;
    for (const auto& im : out.images) {
        if (im.id == s1 || im.id == s2) {
            CHECK(im.w == 166);
            CHECK(im.h == 46);
        }
        if (im.graphic == 210 || im.graphic == 820) ++graphics;
    }
    CHECK(graphics == 2);  // #4000 210 and 820 [BYTES seg10:0x4127 seg10:0x4138]
    std::vector<std::string> names;
    for (const auto& im : out.images)
        if (!im.name.empty()) names.push_back(im.name);
    CHECK(names == std::vector<std::string>{"BW_BLOCKS", "BW_MBLOCKS", "OUT_SIGN"});
    auto has = [&out](const BlitCall& k) {
        for (const BlitCall& c : out.calls)
            if (c == k) return true;
        return false;
    };
    for (DcId s : {s1, s2}) {
        CHECK(has(pat_blt_call(s, 0, 0, 166, 46, kBlackness)));
        // Cell 0's mirror into cell 1: the negative width from x 79 [BYTES seg10:0x41b8].
        BlitCall m = bit_blt_call(s, 79, 6, -34, 34, s, 6, 6, kSrcCopy);
        m.op = BlitCall::kStretchBlt, m.sw = 34, m.sh = 34;
        CHECK(has(m));
        // The overlapping re-mirror as copy-then-write: cell 0's columns 1 ... 29 to 51 ... 79
        // [BYTES seg10:0x41f0] [DESIGN].
        CHECK(has(bit_blt_call(s, 51, 6, 29, 34, s, 7, 6, kSrcCopy)));
    }
    // OUT_SIGN frame 1 (x 26) on cells 0 and 1 of slot 1 only [BYTES seg10:0x4202 seg10:0x4262].
    CHECK(has(bit_blt_call(s1, 8, 16, 26, 13, wg.out_sign, 26, 0, kSrcCopy)));
    CHECK(has(bit_blt_call(s1, 52, 16, 26, 13, wg.out_sign, 26, 0, kSrcCopy)));
    CHECK_FALSE(has(bit_blt_call(s2, 8, 16, 26, 13, wg.out_sign, 26, 0, kSrcCopy)));
    // Only while slot 1 is empty [BYTES seg10:0x3eb1].
    const std::size_t n = out.calls.size();
    build_gate_images(out, images, wg);
    CHECK(out.calls.size() == n);
}

TEST_CASE("wave images: the seg9:0x22b4 chain as the port has it") {
    RecordingBlitter out;
    ImageSlotTable images{};
    std::int16_t dg_740 = 1;
    WaveGraphics saved = wave_graphics();
    wave_graphics() = WaveGraphics{};
    build_wave_images(out, images, dg_740);
    CHECK(images[kSlotRockBig] != kDcNone);  // seg10:0x3a2e
    CHECK(images[kSlotGate] != kDcNone);     // seg10:0x3ea2
    CHECK(wave_graphics().brushes);
    CHECK(wave_graphics().beam[4] != kBrushNone);
    wave_graphics() = saved;
}

TEST_CASE("wave brushes: seg10:0x4b2e, seg10:0x4dfe and the start-up's pattern brushes") {
    RecordingBlitter out;
    WaveGraphics wg;
    create_wave_brushes(out, wg);
    auto colour = [&out](BrushId id) {
        for (const auto& b : out.brushes)
            if (b.id == id) return b.colorref;
        return 0xdeadbeefu;
    };
    // The beam DGROUP:0x3b88[1 ... 5]: 00007f, 007fc4, 00c4ff, 00ffff, ffffff as RRGGBB [BYTES
    // seg10:0x4d44-0x4d84]; the oracle's beam is 00ffff (index 4).
    CHECK(colour(wg.beam[1]) == 0x007F0000u);
    CHECK(colour(wg.beam[2]) == 0x00C47F00u);
    CHECK(colour(wg.beam[3]) == 0x00FFC400u);
    CHECK(colour(wg.beam[4]) == 0x00FFFF00u);
    CHECK(colour(wg.beam[5]) == 0x00FFFFFFu);
    // The heat DGROUP:0x3b74[1 ... 9] [BYTES seg10:0x4b67-0x4bd9].
    CHECK(colour(wg.heat[1]) == 0x0040407Fu);
    CHECK(colour(wg.heat[4]) == 0x001020C0u);
    CHECK(colour(wg.heat[9]) == 0x000072FFu);
    // The stars DGROUP:0x3b4c[10 ... 14], PALETTERGB [BYTES seg10:0x4fda-0x501d].
    CHECK(colour(wg.star[10]) == 0x02FFFF00u);
    CHECK(colour(wg.star[11]) == 0x02E6D700u);
    CHECK(colour(wg.star[12]) == 0x02CDAF00u);
    CHECK(colour(wg.star[13]) == 0x02A08200u);
    CHECK(colour(wg.star[14]) == 0x02826400u);
    // DGROUP:0x5ea8 from DGROUP:0x6c6's rows (13 bits set) [BYTES DGROUP:0x6c6 seg10:0xe4].
    bool mask = false;
    for (const auto& b : out.brushes) {
        if (b.id != wg.mask) continue;
        mask = true;
        CHECK(b.rows == std::array<std::uint8_t, 8>{0x82, 0x10, 0x45, 0x10, 0x84, 0x20, 0x02, 0x90});
        int bits = 0;
        for (std::uint8_t r : b.rows)
            for (int i = 0; i < 8; ++i) bits += (r >> i) & 1;
        CHECK(bits == 13);
    }
    CHECK(mask);
    const std::size_t n = out.brushes.size();
    create_wave_brushes(out, wg);  // once
    CHECK(out.brushes.size() == n);
}

TEST_CASE("gate: wave 1's posts and beam (seg12:0x1776) [ORACLE wave-0001]") {
    GateRig r;
    move_pool8(r.c, r.w);
    // seg8:0xb76((char)r | 0x800, 1, DGROUP:0x39e8[1], 0) for both posts, cells 0 and 1
    // [BYTES seg12:0x19bb seg12:0x19d5].
    REQUIRE(r.host.sprites.size() == 2);
    CHECK(r.host.sprites[0] == Sprite{0x0800, 1, kSlotGate, 0});
    CHECK(r.host.sprites[1] == Sprite{0x0801, 1, kSlotGate, 0});
    CHECK(r.post(0).src_x == 6);
    CHECK(r.post(1).src_x == 46);
    // The beam: index 1 -> 2 (brush 007fc4), the erase at the old (0, 0) of 1 + 150 and the
    // 150 x 2 PATCOPY at post 0 + (34, 15) [BYTES seg12:0x1ab1 seg12:0x1af4 seg12:0x1b24].
    CHECK(r.w.beam_index == 2);
    const std::vector<BlitCall> win = r.window_calls();
    REQUIRE(win.size() == 4);
    CHECK(win[0] == select_call(kDcWindow, 0x302));
    CHECK(win[1] == pat_blt_call(kDcWindow, 0, 0, 151, 2, kBlackness));
    CHECK(win[2] == pat_blt_call(kDcWindow, 557, 179, 150, 2, kPatCopy));
    CHECK(win[3] == select_call(kDcWindow, kBrushWhite));
    CHECK(r.w.gate[0].beam_x == 557);
    CHECK(r.w.gate[0].beam_y == 179);
    // The same position next frame: no erase.
    r.out.calls.clear();
    move_pool8(r.c, r.w);
    const std::vector<BlitCall> win2 = r.window_calls();
    REQUIRE(win2.size() == 3);
    CHECK(win2[1] == pat_blt_call(kDcWindow, 557, 179, 150, 2, kPatCopy));
    // The index runs 2, 3, 4, 5, 4, 3, 2, 1, 2 (period 8) [BYTES seg12:0x17ab-0x17cd].
    std::vector<int> seq{r.w.beam_index};
    for (int k = 0; k < 8; ++k) {
        move_pool8(r.c, r.w);
        seq.push_back(r.w.beam_index);
    }
    CHECK(seq == std::vector<int>{3, 4, 5, 4, 3, 2, 1, 2, 3});
    // No icons left: nothing about the OUT sign yet (icons_b = 3).
    for (const BlitCall& k : r.out.calls) CHECK(k.dst != 0x150);
}

TEST_CASE("gate: the beam's start off screen comes from seg8:0x29ea") {
    GateRig r;
    r.post(0).on_screen = 0;  // post 1 alone on screen
    r.view.x0 = 6000, r.view.y0 = 13000;
    r.view.x0l = 6000, r.view.y0l = 13000;
    r.view.x1 = 6000 + 8192, r.view.y1 = 13000 + 5464;
    move_pool8(r.c, r.w);
    REQUIRE(r.host.sprites.size() == 1);
    CHECK(r.host.sprites[0] == Sprite{0x0801, 1, kSlotGate, 0});
    // ((6152 - 6000) >> 3, (13920 - 13000) >> 3) + (34, 15) = (19 + 34, 115 + 15).
    CHECK(r.w.gate[0].beam_x == 53);
    CHECK(r.w.gate[0].beam_y == 130);
}

TEST_CASE("gate: heated posts draw from slot 2 under DGROUP:0x3b74 (MERGECOPY)") {
    GateRig r;
    r.post(0).flame = 4;  // heat 4 + index 2 - 1 = brush 5
    r.post(1).flame = 7;  // heat 7 - index 2 = brush 5
    move_pool8(r.c, r.w);
    REQUIRE(r.host.sprites.size() == 2);
    CHECK(r.host.sprites[0] == Sprite{0x0800, 1, kSlotGateMask, 1});
    CHECK(r.host.sprites[1] == Sprite{0x0801, 1, kSlotGateMask, 1});
    CHECK(r.out.calls[0] == select_call(kDcWindow, 0x315));
    // The heat drops once +0x6a has passed, with +0x6a = now + 3000 [BYTES seg12:0x1921-0x1940].
    CHECK(r.post(0).flame == 3);
    CHECK(r.post(0).d6a.get() == 53000u);
    // A negative heat climbs by one a frame, with one rand() per post [BYTES seg12:0x195b].
    GateRig n;
    n.post(0).flame = static_cast<std::uint8_t>(-3);
    const std::uint32_t before = n.rng.state();
    move_pool8(n.c, n.w);
    CHECK(static_cast<std::int8_t>(n.post(0).flame) == -2);
    is::shell::Rng ref;
    ref.set_state(before);
    ref.rand();
    CHECK(n.rng.state() == ref.state());
}

TEST_CASE("gate: a moving gate closes and bounces (seg12:0x1776, kind 7 stops)") {
    GateRig r;
    r.w.gate[0].w12 = 1;  // DGROUP:0x668a
    move_pool8(r.c, r.w);
    CHECK(r.post(0).x == 6160);
    CHECK(r.post(0).vx == 8);
    CHECK(r.post(1).x == 7616);
    CHECK(r.post(1).vx == -8);
    CHECK(r.w.gate[0].beam.l == 6432);
    CHECK(r.w.gate[0].beam.r == 7616);
    // The erase is the gap + 16 taken on r1 before r1 moves (r0 has moved): (7624 - 272 - 6160
    // + 16) >> 3 + 1 = 152; the draw the gap after both moves, 1184 >> 3 [BYTES seg12:0x1ed6
    // seg12:0x1f33 seg12:0x1ab1].
    const std::vector<BlitCall> win = r.window_calls();
    REQUIRE(win.size() == 4);
    CHECK(win[1] == pat_blt_call(kDcWindow, 0, 0, 152, 2, kBlackness));
    CHECK(win[2] == pat_blt_call(kDcWindow, 557, 179, 148, 2, kPatCopy));
    // Below the minimum DGROUP:0x6686 the speed turns, in a duel it stops [BYTES seg12:0x184c
    // seg12:0x185c].
    GateRig b;
    b.w.gate[0].w12 = 1;
    b.w.gate[0].w0e = 1190;
    move_pool8(b.c, b.w);
    CHECK(b.w.gate[0].w12 == -1);
    GateRig d;
    d.w.kind = 7;
    d.w.gate[0].w12 = 1;
    d.w.gate[0].w0e = 1190;
    move_pool8(d.c, d.w);
    CHECK(d.w.gate[0].w12 == 0);
}

TEST_CASE("gate: the OUT sign flashes every 400 ms once the icons are gone") {
    GateRig r;
    r.w.icons_b = 0;  // DGROUP:0x91fc
    move_pool8(r.c, r.w);
    CHECK(r.w.out_frame == 1);
    CHECK(r.w.out_due == 50400u);
    std::vector<BlitCall> flash;
    for (const BlitCall& k : r.out.calls)
        if (k.dst == 0x150) flash.push_back(k);
    REQUIRE(flash.size() == 2);
    // ((2 << Size) >> 1 + gap, (10 << Size) >> 1 + gap) and (2 gap + size + 7, same) from x 26
    // [BYTES seg12:0x1c0e seg12:0x1c64].
    CHECK(flash[0] == bit_blt_call(0x150, 8, 16, 26, 13, 0x151, 26, 0, kSrcCopy));
    CHECK(flash[1] == bit_blt_call(0x150, 53, 16, 26, 13, 0x151, 26, 0, kSrcCopy));
    r.out.calls.clear();
    r.g.now = 50300;  // not yet
    move_pool8(r.c, r.w);
    for (const BlitCall& k : r.out.calls) CHECK(k.dst != 0x150);
    r.g.now = 50401;
    move_pool8(r.c, r.w);
    CHECK(r.w.out_frame == 0);
    bool frame0 = false;
    for (const BlitCall& k : r.out.calls)
        if (k.dst == 0x150 && k.sx == 0) frame0 = true;
    CHECK(frame0);
}

TEST_CASE("gate: shot heat on a post (seg12:0x1776's collision part)") {
    GateRig r;
    r.w.pools.rec(kPool5, 0).mass = 0x78;
    r.host.hit = static_cast<std::uint16_t>(kPool5 << 8);
    move_pool8(r.c, r.w);
    CHECK(r.post(0).flame == 3);  // + 1 [BYTES seg12:0x1d06]
    CHECK(r.post(0).d6a.get() == 55000u);
    CHECK(r.host.posts_hit == 2);
    CHECK(r.f.dg_943a == kPoolGatePosts);
    CHECK(r.f.dg_943c == 1);
}

TEST_CASE("gate: the passage test seg12:0x1f7a") {
    GateRig r;
    ObjectRecord& me = r.w.pools.rec(kPoolShips, 0);
    me.state = 0;
    me.extent = 256;
    me.x = 6800, me.y = 13900;  // meets the beam rectangle (6424, 14032, 7624, 14080)
    me.scr_x = 600, me.scr_y = 150, me.size = 32;
    r.host.vis.count = 1;
    r.host.vis.pool[0] = kPoolShips;
    r.host.vis.rec[0] = 0;
    // The player's +0x88 is 49800: 49800 + 300 is not before 50000.
    r.w.ships[0].d88.set(49800u);
    CHECK(gate_test(r.c, r.w, 0) == 0xffffu);
    r.w.ships[0].d88.set(49600u);
    CHECK(gate_test(r.c, r.w, 0) == (kPoolShips << 8));
    // Byte +0x2e = 1 hides it unless ship word +0x2e = 2 [BYTES seg12:0x20f0].
    me.b2e = 1;
    CHECK(gate_test(r.c, r.w, 0) == 0xffffu);
    set_word_at(r.w.ships[0], 0x2e, 2);
    CHECK(gate_test(r.c, r.w, 0) == (kPoolShips << 8));
    me.b2e = 0;
    // A post's mass 0xdac never passes.
    r.host.vis.pool[0] = kPoolGatePosts;
    CHECK(gate_test(r.c, r.w, 0) == 0xffffu);
    // In the mover: the player passes, seg12:0x2288 and DGROUP:0x1e80 = now + 1000.
    r.host.vis.pool[0] = kPoolShips;
    move_pool8(r.c, r.w);
    REQUIRE_FALSE(r.host.passed.empty());
    CHECK(r.host.passed[0] == std::array<int, 2>{kPoolShips, 0});
    CHECK(r.w.w1e80 == 51000u);
    CHECK(r.host.sounds.empty());
    // Another ship: its square blacked and sound 0x27 [BYTES seg12:0x1e81 seg12:0x1eb2].
    GateRig o;
    o.g.player = 1;
    ObjectRecord& s0 = o.w.pools.rec(kPoolShips, 0);
    s0 = me;
    o.w.ships[0].d88.set(40000u);
    o.host.vis.count = 1;
    o.host.vis.pool[0] = kPoolShips;
    o.host.vis.rec[0] = 0;
    move_pool8(o.c, o.w);
    REQUIRE_FALSE(o.host.sounds.empty());
    CHECK(o.host.sounds[0] == std::array<int, 2>{0x27, kPoolShips * 256});
    bool black = false;
    for (const BlitCall& k : o.out.calls)
        if (k == pat_blt_call(kDcWindow, 600, 150, 32, 32, kBlackness)) black = true;
    CHECK(black);
}

TEST_CASE("gate: a computer ship's hand-off tests |beam.r - beam.l - 480| <= 16 [BYTES seg12:0x216e-0x218c]") {
    auto run = [](std::int16_t speed, std::int16_t beam_w) {
        GateRig r;
        r.g.player = 0;
        ObjectRecord& s1 = r.w.pools.rec(kPoolShips, 1);
        s1.state = 0;
        s1.extent = 256;
        s1.x = 6000, s1.y = 12000;  // off the beam rectangle
        s1.vx = 5, s1.vy = 6;
        set_word_at(r.w.ships[1], 0x32, 12);
        set_word_at(r.w.ships[1], 0x34, 0);  // the gate's centre record (post 0)
        set_word_at(r.w.ships[1], 0x38, 2);
        r.w.pools.rec(kPoolGateCentres, 0).x = 6200;
        r.w.pools.rec(kPoolGateCentres, 0).y = 12100;
        r.w.gate[0].w12 = speed;
        r.w.gate[0].beam.r = static_cast<std::int16_t>(r.w.gate[0].beam.l + beam_w);
        r.host.vis.count = 1;
        r.host.vis.pool[0] = kPoolShips;
        r.host.vis.rec[0] = 1;
        CHECK(gate_test(r.c, r.w, 0) == 0xffffu);
        CHECK(s1.vx == 0);  // stopped whatever the speed [BYTES seg12:0x2142 seg12:0x2168]
        CHECK(s1.vy == 0);
        const bool handed = !r.host.ai.empty();
        if (handed) {
            CHECK(word_at(r.w.ships[1], 0x34) == 1);
            CHECK(word_at(r.w.ships[1], 0x08) == 1);
        }
        return handed;
    };
    CHECK(run(0, 1200));        // a standing gate
    CHECK_FALSE(run(2, 480));   // opening: never
    CHECK(run(-2, 480));        // closing, at 480
    CHECK(run(-2, 496));        // |16| passes
    CHECK(run(-2, 464));
    CHECK_FALSE(run(-2, 497));  // 17 does not
    CHECK_FALSE(run(-2, 1200)); // the speed itself is not the test
}

TEST_CASE("pool-8 mover: the kind dispatch and the frame toggles (seg12:0x633e)") {
    GateRig r;
    r.w.pools.set_in_use(kPoolGatePosts, 5);
    r.w.pool8[2].at(0x1c) = 1;
    r.w.pool8[3].at(0x1c) = 2;
    r.w.pool8[4].at(0x1c) = 2;
    r.w.pools.rec(kPoolGatePosts, 4).state = 1;  // not in play: skipped
    for (int k = 2; k < 5; ++k) r.w.pools.rec(kPoolGatePosts, k).w1c = 7;
    move_pool8(r.c, r.w);
    CHECK(r.host.hazards == std::vector<std::array<int, 2>>{{1, 2}, {2, 3}});
    CHECK(r.w.pools.rec(kPoolGatePosts, 2).w1c == 0);
    CHECK(r.w.pools.rec(kPoolGatePosts, 4).w1c == 7);
    // DGROUP:0x94a0 = (Detail + 1) / 2 + 2; the 0/1 toggle; the oscillator 2 + 1 = 3.
    CHECK(r.w.w94a0 == 4);
    CHECK(r.w.w1c32 == 1);
    CHECK(r.w.w0d08 == 3);
    CHECK(r.w.w949e == 1);
    move_pool8(r.c, r.w);
    move_pool8(r.c, r.w);  // 3 -> 4 -> 5: past 4 the step turns
    CHECK(r.w.w0d08 == 5);
    CHECK(r.w.w0d06 == -1);
}

TEST_CASE("frame: with a World the frame runs the pool-8 mover, else the host's stub") {
    GateRig r;
    wave_frame(r.c);
    CHECK(r.host.sprites.size() == 2);  // the posts
    // Without a World (the update tests' Recorder) the host's mover(k12_633e) runs.
    r.host.w = nullptr;
    r.host.sprites.clear();
    wave_frame(r.c);
    CHECK(r.host.sprites.empty());
}
