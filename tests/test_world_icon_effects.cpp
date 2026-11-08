// The icon mover's helpers (src/world/icon_effects.h) and their pictures (src/hud/icon_images.h)
// (M4 stage 5, track ICONS; work/analysis/m4/s5-icons.md sections 1 and 4 with
// s5-icons-verify.md): the melt and the burn through the mover, the drip columns, the burnt-out
// end, the pool-5 FIREBALL and SNOWBALL cases, the overlay, the backup and the briefing's crack
// loop seg9:0x5d70 (triage M-3).
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "doctest.h"
#include "hud/icon_images.h"
#include "nav/fs.h"
#include "shell/rng.h"
#include "world/icon_effects.h"
#include "world/icon_mover.h"
#include "world/setup.h"

using namespace is::world;
namespace nav = is::nav;

namespace {

using Call3 = std::tuple<int, int, int>;
using Call4 = std::tuple<int, int, int, int>;

class FxCollide : public CollideHost {
public:
    std::vector<std::pair<int, int>> rows, gone, goals;
    std::vector<Call4> erased;
    std::vector<CrackPlan> cracks;
    void column_row(int p, int r) override { rows.emplace_back(p, r); }
    void icon_gone(int r, int a) override { gone.emplace_back(r, a); }
    void erase(int x, int y, int w, int h) override { erased.emplace_back(x, y, w, h); }
    void seg13_b6f2(int s, int c) override { goals.emplace_back(s, c); }
    void icon_crack(const CrackPlan& p) override { cracks.push_back(p); }
};

class FxHost : public UpdateHost {
public:
    World* world = nullptr;
    FxCollide rc;
    std::vector<Call3> puffs;
    std::vector<Call4> sounds;
    World* frame_world(Pools& p) override { return world && &world->pools == &p ? world : nullptr; }
    std::uint16_t collide(std::int16_t) override { return 0xffffu; }
    void draw_sprite(std::int16_t, int, int, int) override {}
    CollideHost& collide_host() override { return rc; }
    void puff(int pool, int rec, int arg) override { puffs.emplace_back(pool, rec, arg); }
    void sound(int id, int pool, int rec, int mode) override { sounds.emplace_back(id, pool, rec, mode); }
    Blitter* blitter() override { return nullptr; }
};

// The plans the drawing side would get.
struct Sink {
    std::vector<IconPlan> plans;
    Sink() {
        clear_icon_plans();
        defer_icon_plans(false);
        icon_effect_links().draw = [this](const IconPlan& p) { plans.push_back(p); };
    }
    ~Sink() { icon_effect_links().draw = nullptr; }
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    template <typename T>
    std::vector<T> all() const {
        std::vector<T> v;
        for (const IconPlan& p : plans)
            if (const T* q = std::get_if<T>(&p)) v.push_back(*q);
        return v;
    }
};

// One wave with icon 2 in play at S = 32, g = 6, Size 1.
struct Rig {
    World w;
    WaveGlobals g;
    FrameGlobals f;
    ViewState view;
    KeyLatch keys;
    JoyState joy;
    JoyButtons jb;
    is::shell::Rng rng;
    FxHost host;
    UpdateCtx c{FlightCtx{w.pools, w.ships, g, keys, host, rng}, f, view, host, joy, jb};
    Rig() {
        host.world = &w;
        rng.srand(777);
        g.now = 50000;
        g.size = 1;
        w.pools.resize(kPoolIcons, 8);
        w.pools.set_in_use(kPoolIcons, 8);
        w.icons_a = 3;
        for (int i = 0; i < 8; ++i) {
            ObjectRecord& o = w.pools.rec(kPoolIcons, i);
            o.size = 32, o.gap = 6, o.b53 = 1, o.mass = 160, o.state = 1;
        }
        ObjectRecord& o = w.pools.rec(kPoolIcons, 2);
        o.state = 0, o.on_screen = 1, o.scr_x = 100, o.scr_y = 120;
        w.pools.resize(5, 2);
        w.pools.resize(kPoolShips, 4);
    }
    ObjectRecord& icon() { return w.pools.rec(kPoolIcons, 2); }
    IconRecord& ic() { return w.icons[2]; }
    std::int8_t heat() { return static_cast<std::int8_t>(icon().flame); }
};

is::shell::Rng advanced(std::uint32_t state, int n) {
    is::shell::Rng r;
    r.set_state(state);
    for (int k = 0; k < n; ++k) r.rand();
    return r;
}

}  // namespace

TEST_CASE("icon effects: seg7:0x4d86's columns at S 32, g 6, icon 2 [BYTES seg7:0x4da7-0x4e0d seg7:0x4ed4-0x4ef0]") {
    // s5-icons.md Port plan check (1): rand() 100, 200, 300, 400 give r1 = 14, r2 = 18, r3 = 0,
    // r4 = 22 at Size 1; cell x 82; each copy 2 wide, S high, from y = g - d = 4.
    const IconDripPlan a = drip_plan(2, 32, 6, 1, false, 100, 200, 300, 400, 0);
    CHECK(a.count == 4);
    CHECK_FALSE(a.own);
    CHECK(a.x == std::array<std::int16_t, 4>{96, 100, 82, 104});
    CHECK(a.r == std::array<std::int16_t, 4>{14, 18, 0, 22});
    CHECK(a.width == 2);
    CHECK(a.from_y == 4);
    // The burn: r1 and r3, r3 = r4 when bit 0 of the fifth draw is set; from y = g - d / 2.
    const IconDripPlan b = drip_plan(2, 32, 6, 1, true, 100, 200, 300, 400, 1);
    CHECK(b.count == 2);
    CHECK(b.own);
    CHECK(b.x[0] == 96);
    CHECK(b.x[1] == 104);
    CHECK(b.from_y == 5);
    const IconDripPlan b2 = drip_plan(2, 32, 6, 1, true, 100, 200, 300, 400, 2);
    CHECK(b2.x[1] == 82);
    // Size 2: r3 = (300 % 10) << 2 has no shift right; d = 4; at Size 0 d = 1 and d / 2 = 0.
    const IconDripPlan c = drip_plan(0, 64, 12, 2, false, 101, 0, 7, 9, 0);
    CHECK(c.r[0] == 30);  // ((101 % 12 + 10) << 2) >> 1 = (5 + 10) * 2
    CHECK(c.r[2] == 28);
    CHECK(c.r[3] == 62);  // (9 + 22) * 2
    CHECK(c.width == 4);
    CHECK(drip_plan(0, 16, 3, 0, true, 0, 0, 0, 0, 0).from_y == 3);
}

TEST_CASE("icon effects: a melt runs 74 mover iterations of 4 draws, then seg7:0x4ffe [BYTES seg7:0x322d-0x324b seg7:0x4ffe-0x5084]") {
    Rig r;
    Sink sink;
    r.ic().w00 = 80;  // a hot rock's contact [BYTES seg7:0x4ce0]
    const std::uint32_t start = r.rng.state();
    int frames = 0;
    while (r.icon().state == 0 && frames < 500) {
        move_icons(r.c, r.w);
        ++frames;
    }
    CHECK(frames == 74);
    const std::vector<IconDripPlan> d = sink.all<IconDripPlan>();
    REQUIRE(d.size() == 74u);
    for (const IconDripPlan& p : d) {
        CHECK(p.icon == 2);
        CHECK(p.count == 4);
        CHECK_FALSE(p.own);
        CHECK(p.from_y == 4);
    }
    CHECK(r.rng.state() == advanced(start, 74 * 4).state());
    // seg7:0x4ffe: the square blacked, state 1, two puffs, the counters, the column row.
    CHECK(r.ic().w00 == 0);
    CHECK(r.icon().state == 1);
    CHECK(r.host.rc.erased == std::vector<Call4>{{100, 120, 32, 32}});
    CHECK(r.host.puffs == std::vector<Call3>{{0, 2, 1}, {0, 2, 1}});
    CHECK(r.host.rc.gone == std::vector<std::pair<int, int>>{{2, 0}});
    REQUIRE_FALSE(r.host.rc.rows.empty());
    CHECK(r.host.rc.rows.back() == std::pair<int, int>{0, 2});
    // No smoke in the melt, and nothing more once the icon is out of play.
    move_icons(r.c, r.w);
    CHECK(sink.all<IconDripPlan>().size() == 74u);
}

TEST_CASE("icon effects: a burn runs 163 iterations of 5 draws with 48 puffs [BYTES seg7:0x4fc1-0x4fef seg7:0x31dc-0x31f8]") {
    // s5-icons.md 1.8: a pool-5 hit leaves w02 = 3 for the first step; 89 -> 6 takes
    // 4 + 15 * 10 + 5 + 4 = 163 calls, a puff seg7:0x5b68(0, i, 0) after each call that leaves
    // w00 < 50 with w00 % 5 = 0 (45 ... 10, six calls each).
    Rig r;
    Sink sink;
    r.ic().w00 = 89;
    r.ic().w02 = 3;
    const std::uint32_t start = r.rng.state();
    int frames = 0;
    while (r.icon().state == 0 && frames < 500) {
        move_icons(r.c, r.w);
        ++frames;
    }
    CHECK(frames == 163);
    const std::vector<IconDripPlan> d = sink.all<IconDripPlan>();
    REQUIRE(d.size() == 163u);
    for (const IconDripPlan& p : d) {
        CHECK(p.own);
        CHECK(p.count == 2);
        CHECK(p.from_y == 5);
    }
    CHECK(r.rng.state() == advanced(start, 163 * 5).state());
    int smoke = 0, puff = 0;
    for (const Call3& p : r.host.puffs) {
        CHECK(std::get<0>(p) == 0);
        CHECK(std::get<1>(p) == 2);
        (std::get<2>(p) == 0 ? smoke : puff) += 1;
    }
    CHECK(smoke == 48);
    CHECK(puff == 2);  // seg7:0x4ffe's two
    CHECK(r.icon().state == 1);
}

TEST_CASE("icon effects: the mover thaws at heat -2 through seg7:0x5660 [BYTES seg7:0x3193-0x31d8 seg7:0x566d-0x56af]") {
    Rig r;
    Sink sink;
    r.icon().flame = 0xfd;  // -3
    r.f.dg_9142 = 30;
    move_icons(r.c, r.w);
    CHECK(r.heat() == -2);
    const std::vector<IconRestorePlan> v = sink.all<IconRestorePlan>();
    REQUIRE(v.size() == 1u);
    CHECK(v[0].icon == 2);
    CHECK(v[0].strip_x == 82);
    CHECK(v[0].size == 32);
    // seg7:0x233c(0, i, 0) at heat -2 only redraws the row: no crack, `damage` untouched.
    CHECK(r.host.rc.cracks.empty());
    CHECK(r.host.rc.rows == std::vector<std::pair<int, int>>{{0, 2}});
}

TEST_CASE("icon effects: SNOWBALL freezes, FIREBALL thaws (seg7:0x4a38's pool-5 cases) [BYTES seg7:0x4aa1-0x4b24 seg7:0x4c27-0x4c4f]") {
    Rig r;
    Sink sink;
    r.w.pools.rec(5, 0).mass = 0xc8;  // SNOWBALL
    r.w.pools.rec(5, 1).mass = 0x78;  // FIREBALL
    r.host.collide_globals().pool5[1] = Pool5Entry{kPoolShips, 3, 0, 0};
    icon_pool5_hit(r.c, r.w, 0, 2);
    CHECK(r.heat() == -100);
    CHECK(r.icon().b53 == 0);
    CHECK(r.w.dir().byte(0x1c8 + 2) == 2);
    const std::vector<IconOverlayPlan> o = sink.all<IconOverlayPlan>();
    REQUIRE(o.size() == 1u);
    CHECK(o[0].icon == 2);
    CHECK(o[0].cell == 1);
    CHECK(o[0].x == 82);
    CHECK(r.host.rc.rows == std::vector<std::pair<int, int>>{{0, 2}});
    // A second SNOWBALL on the frozen icon does nothing.
    icon_pool5_hit(r.c, r.w, 0, 2);
    CHECK(sink.all<IconOverlayPlan>().size() == 1u);
    // FIREBALL: four puffs, sound 0x76 below heat -2, heat -2, the backup back, the shooter's words.
    r.host.rc.rows.clear();
    icon_pool5_hit(r.c, r.w, 1, 2);
    CHECK(r.heat() == -2);
    CHECK(r.host.puffs == std::vector<Call3>(4, Call3{0, 2, 1}));
    CHECK(r.host.sounds == std::vector<Call4>{{0x76, 0, 2, 1}});
    CHECK(sink.all<IconRestorePlan>().size() == 1u);
    CHECK(r.host.rc.rows == std::vector<std::pair<int, int>>{{0, 2}});
    CHECK(r.w.ships[3].w36 == 2);
    CHECK(r.w.ships[3].w38 == 2);
    CHECK(r.icon().b53 == 0);  // capturable again only when the heat reaches 0 (the mover)
    // Again at heat -2: no sound.
    icon_pool5_hit(r.c, r.w, 1, 2);
    CHECK(r.host.sounds.size() == 1u);
    CHECK(r.host.puffs.size() == 8u);
}

TEST_CASE("icon effects: FIREBALL ignites, SNOWBALL puts it out [BYTES seg7:0x4b2b-0x4ba9 seg7:0x4bc3-0x4c21 seg7:0x477e-0x47b4]") {
    Rig r;
    Sink sink;
    r.w.pools.rec(5, 0).mass = 0x78;
    r.w.pools.rec(5, 1).mass = 0xc8;
    // A computer ship (the literal pool 14, ship 1) on its way to this icon.
    r.host.collide_globals().pool5[0] = Pool5Entry{0xe, 1, 0, 0};
    r.w.ships[1].w32 = 0;
    r.w.ships[1].w34 = 2;
    r.w.ships[1].w38 = 1;
    icon_pool5_hit(r.c, r.w, 0, 2);
    CHECK(r.ic().w02 == 2);
    CHECK(r.ic().w00 == 89);
    CHECK(icon_burn_until(r.w, 2) == 54000u);
    CHECK(r.icon().b53 == 0);
    CHECK(r.host.rc.goals == std::vector<std::pair<int, int>>{{1, 1}});
    CHECK(r.host.sounds == std::vector<Call4>{{0x6a, 0, 2, 1}});
    const std::vector<IconFlamePlan> f = sink.all<IconFlamePlan>();
    REQUIRE(f.size() == 1u);
    CHECK(f[0].icon == 2);
    CHECK(f[0].h12 == 12);
    CHECK(f[0].strip_x == 82);
    // A FIREBALL on a burning icon does nothing more.
    icon_pool5_hit(r.c, r.w, 0, 2);
    CHECK(sink.all<IconFlamePlan>().size() == 1u);
    CHECK(r.host.rc.goals.size() == 1u);
    // SNOWBALL: out, heat -2, four puffs, capturable, sound 0x76, the melted cell into the backup.
    r.ic().w02 = 5;
    icon_pool5_hit(r.c, r.w, 1, 2);
    CHECK(r.ic().w00 == 0);
    CHECK(r.ic().w02 == 0);
    CHECK(r.heat() == -2);
    CHECK(r.icon().b53 == 1);
    CHECK(r.host.puffs == std::vector<Call3>(4, Call3{0, 2, 1}));
    CHECK(r.host.sounds.back() == Call4{0x76, 0, 2, 1});
    const std::vector<IconBackupPlan> b = sink.all<IconBackupPlan>();
    REQUIRE(b.size() == 1u);
    CHECK(b[0].strip_x == 82);
    // A SNOWBALL on an icon with heat < 0 and no burn does nothing.
    icon_pool5_hit(r.c, r.w, 1, 2);
    CHECK(sink.all<IconOverlayPlan>().empty());
    // The goal call needs `damage` 0: a damaged icon ignites without it.
    Rig q;
    Sink s2;
    q.w.pools.rec(5, 0).mass = 0x78;
    q.host.collide_globals().pool5[0] = Pool5Entry{0xe, 1, 0, 0};
    q.w.ships[1].w34 = 2;
    q.w.ships[1].w38 = 1;
    q.icon().damage = 3;
    icon_pool5_hit(q.c, q.w, 0, 2);
    CHECK(q.host.rc.goals.empty());
    CHECK(q.ic().w00 == 89);
}

TEST_CASE("icon effects: seg7:0x2512 pastes only while w00 is 0 [BYTES seg7:0x2521-0x252a seg7:0x2612-0x2642]") {
    Rig r;
    Sink sink;
    CollideCtx x = frame_collide_ctx(r.c, r.w);
    r.ic().w00 = 5;
    icon_overlay(x, 2, 0);
    CHECK(sink.plans.empty());
    CHECK(r.w.dir().byte(0x1c8 + 2) == 0);
    CHECK(r.icon().b53 == 1);
    CHECK(r.host.rc.rows == std::vector<std::pair<int, int>>{{0, 2}});
    r.ic().w00 = 0;
    r.icon().size = 16;  // record i's own S and g
    r.icon().gap = 3;
    icon_overlay(x, 2, 0);
    const std::vector<IconOverlayPlan> o = sink.all<IconOverlayPlan>();
    REQUIRE(o.size() == 1u);
    CHECK(o[0].cell == 0);
    CHECK(o[0].x == 41);  // (3 + 16)·2 + 3
    CHECK(o[0].size == 16);
    CHECK(r.w.dir().byte(0x1c8 + 2) == 1);
    CHECK(r.icon().b53 == 2);
}

TEST_CASE("icon effects: the plans on the Blitter (src/hud/icon_images.h)") {
    RecordingBlitter out;
    ImageSlotTable images{};
    is::hud::IconImages ii(out, images);
    const DcId strip = 0x50;
    ii.strip = [] { return DcId{0x50}; };
    // seg7:0x4d86's melt: four strip copies.
    ii.draw(drip_plan(2, 32, 6, 1, false, 100, 200, 300, 400, 0));
    REQUIRE(out.calls.size() == 4u);
    CHECK(out.calls[0] == bit_blt_call(strip, 96, 6, 2, 32, strip, 96, 4, kSrcCopy));
    CHECK(out.calls[3] == bit_blt_call(strip, 104, 6, 2, 32, strip, 104, 4, kSrcCopy));
    // seg7:0x477e: slot 14 + 2 made (5S + 6g) x (2g + S), blacked, the 5 cells.
    out.calls.clear();
    IconFlamePlan f;
    f.icon = 2, f.size = 32, f.gap = 6, f.h12 = 12, f.strip_x = 82;
    ii.draw(f);
    const DcId own = images[static_cast<std::size_t>(kIconOwnSlot + 2)];
    REQUIRE(own != kDcNone);
    REQUIRE(out.images.size() == 3u);  // the graphic, its mask, the image
    CHECK(out.images[0].graphic == kGraphicIconFlames);
    CHECK(out.images[1].name == "mask");
    CHECK(out.images[2].w == 196);
    CHECK(out.images[2].h == 44);
    REQUIRE(out.calls.size() == 16u);
    CHECK(out.calls[0] == pat_blt_call(own, 0, 0, 196, 44, kBlackness));
    CHECK(out.calls[1] == bit_blt_call(own, 6, 18, 32, 20, strip, 82, 18, kSrcCopy));
    CHECK(out.calls[2] == bit_blt_call(own, 6, 6, 32, 32, out.images[1].id, 0, 0, kSrcAnd));
    CHECK(out.calls[3] == bit_blt_call(own, 6, 6, 32, 32, out.images[0].id, 0, 0, kSrcPaint));
    CHECK(out.calls[15] == bit_blt_call(own, 158, 6, 32, 32, out.images[0].id, 128, 0, kSrcPaint));
    CHECK(out.deleted == std::vector<DcId>{out.images[1].id, out.images[0].id});
    // A second ignition reuses the image.
    ii.draw(f);
    CHECK(images[static_cast<std::size_t>(kIconOwnSlot + 2)] == own);
    CHECK(out.images.size() == 5u);
    // The burn's drip: the strip's two columns, then each of the five cells'.
    out.calls.clear();
    ii.draw(drip_plan(2, 32, 6, 1, true, 100, 200, 300, 400, 1));
    REQUIRE(out.calls.size() == 12u);
    CHECK(out.calls[2] == bit_blt_call(own, 6 + 14, 6, 2, 32, own, 6 + 14, 5, kSrcCopy));
    CHECK(out.calls[11] == bit_blt_call(own, 158 + 22, 6, 2, 32, own, 158 + 22, 5, kSrcCopy));
    // seg10:0x506c, then seg7:0x56ba and seg7:0x5660 on slot 32.
    out.calls.clear();
    ii.draw(IconBackupAllPlan{8});
    const DcId bk = images[static_cast<std::size_t>(kIconBackupSlot)];
    REQUIRE(bk != kDcNone);
    CHECK(out.images.back().w == 256);
    CHECK(out.images.back().h == 32);
    REQUIRE(out.calls.size() == 8u);
    CHECK(out.calls[7] == bit_blt_call(bk, 224, 0, 32, 32, strip, 272, 6, kSrcCopy));
    out.calls.clear();
    ii.draw(IconBackupPlan{2, 32, 6, 82});
    ii.draw(IconRestorePlan{2, 32, 6, 82});
    REQUIRE(out.calls.size() == 2u);
    CHECK(out.calls[0].op == BlitCall::kStretchBlt);
    CHECK(out.calls[0].dst == bk);
    CHECK(out.calls[0].x == 64);
    CHECK(out.calls[0].src == strip);
    CHECK(out.calls[0].sx == 82);
    CHECK(out.calls[1].dst == strip);
    CHECK(out.calls[1].x == 82);
    CHECK(out.calls[1].y == 6);
    CHECK(out.calls[1].src == bk);
    CHECK(out.calls[1].sx == 64);
    CHECK(out.calls[1].sw == 32);
    // seg7:0x2512: graphic 360, the mask SRCAND, the graphic SRCPAINT from cell k.
    out.calls.clear();
    IconOverlayPlan o;
    o.icon = 2, o.cell = 1, o.size = 32, o.gap = 6, o.x = 82;
    ii.draw(o);
    REQUIRE(out.calls.size() == 2u);
    const std::size_t n = out.images.size();
    CHECK(out.images[n - 2].graphic == kGraphicIconOverlay);
    CHECK(out.calls[0] == bit_blt_call(strip, 82, 6, 32, 32, out.images[n - 1].id, 32, 0, kSrcAnd));
    CHECK(out.calls[1] == bit_blt_call(strip, 82, 6, 32, 32, out.images[n - 2].id, 32, 0, kSrcPaint));
    // seg7:0x49b6 releases slots 14 ... 21, not the backup.
    ii.release();
    CHECK(images[static_cast<std::size_t>(kIconOwnSlot + 2)] == kDcNone);
    CHECK(out.deleted.back() == own);
    CHECK(images[static_cast<std::size_t>(kIconBackupSlot)] == bk);
    // A briefing crack goes to the crack picture.
    int cracks = 0;
    ii.crack = [&cracks](const CrackPlan&) { ++cracks; };
    ii.draw(CrackPlan{});
    CHECK(cracks == 1);
}

TEST_CASE("icon effects: the plans wait while deferred and flush in order") {
    Sink sink;
    defer_icon_plans(true);
    icon_draw(IconBackupAllPlan{8});
    icon_draw(CrackPlan{});
    CHECK(sink.plans.empty());
    CHECK(pending_icon_plans().size() == 2u);
    flush_icon_plans();
    REQUIRE(sink.plans.size() == 2u);
    CHECK(std::holds_alternative<IconBackupAllPlan>(sink.plans[0]));
    CHECK(std::holds_alternative<CrackPlan>(sink.plans[1]));
    CHECK(pending_icon_plans().empty());
    icon_draw(IconBackupAllPlan{8});  // no longer deferred
    CHECK(sink.plans.size() == 3u);
}

namespace {

class Icons : public IconReader {
public:
    std::map<std::string, int> counts;
    int count(const std::string& path) override {
        auto it = counts.find(path);
        return it == counts.end() ? 0 : it->second;
    }
    std::optional<is::fmt::IconCell> cell(const std::string& path, int index) override {
        auto it = counts.find(path);
        if (it == counts.end() || index >= it->second) return std::nullopt;
        is::fmt::IconCell c;
        c.bgra[static_cast<std::size_t>(1 * is::fmt::kIconCell + 1)] = 0xffffffffu;
        return c;
    }
};

// A revisited directory with three icons (ALPHA.EXE 2, GAMMA.DLL 1).
struct Briefing {
    nav::MemFs fs;
    nav::Walker walker{fs};
    Icons icons;
    is::shell::Rng rng;
    World w;
    SetupHost host{rng, walker, icons};
    Briefing() {
        fs.add_drive('D');
        fs.add_dir("D:\\GAMES");
        fs.add_file("D:\\GAMES\\ALPHA.EXE");
        fs.add_file("D:\\GAMES\\GAMMA.DLL");
        icons.counts["D:\\GAMES\\ALPHA.EXE"] = 2;
        icons.counts["D:\\GAMES\\GAMMA.DLL"] = 1;
        host.now = [] { return 1000u; };
        power_on(w);
        rng.srand(4321);
        w.W = 640;
        w.H = 400;
        w.size = 1;
        w.shift = 3;
        w.detail = 3;
        w.handover.directory = "D:\\GAMES\\";
        w.added = 1;
        w.record = 0;
        w.dir();
        w.wave = 1;
        state1_load(w, host);
        new_game(w);
        w.wave = 1;
    }
};

}  // namespace

TEST_CASE("icon effects: the briefing's seg7:0x233c(0, k, 0) loop writes the -1 slots (M-3) [BYTES seg9:0x5d6b-0x5d8b]") {
    // s5-icons.md 4.3 (verified claims 50, 51, 57-59): records the walk does not activate keep the
    // -1 of the directory's first seg9:0xb2a and carry it into slots 3 ... 7; a damaged icon gets
    // its slot and its cracks (frame 54 / 70 = 0) on the unscaled strip.
    Briefing b;
    Sink sink;
    b.w.added = 0;  // a revisited directory (Navigation Enter found the record)
    b.w.dir().set_word(0x1d2 + 2 * 2, 54);
    for (int k = 3; k < 8; ++k) b.w.pools.rec(kPoolIcons, k).damage = -1;
    enter_briefing(b.w, b.host);
    CHECK(b.w.dir().word(0x1d2 + 0) == 0);
    CHECK(b.w.dir().word(0x1d2 + 2) == 0);
    CHECK(b.w.dir().word(0x1d2 + 4) == 54);
    for (int k = 3; k < 8; ++k) CHECK(b.w.dir().word(0x1d2 + 2 * static_cast<std::size_t>(k)) == -1);
    CHECK(b.w.pools.rec(kPoolIcons, 2).damage == 54);
    // The plans wait for the strip: the backup first, then the one crack.
    CHECK(sink.plans.empty());
    const std::vector<IconPlan>& pending = pending_icon_plans();
    REQUIRE(pending.size() >= 2u);
    REQUIRE(std::holds_alternative<IconBackupAllPlan>(pending[0]));
    CHECK(std::get<IconBackupAllPlan>(pending[0]).count == 8);
    int cracks = 0;
    for (const IconPlan& p : pending) {
        const CrackPlan* c = std::get_if<CrackPlan>(&p);
        if (!c) continue;
        ++cracks;
        CHECK(c->rec == 2);
        CHECK(c->frame == 0);
        CHECK_FALSE(c->scaled);
        CHECK(c->gap == 6);
        CHECK(c->side == 32);
        CHECK(c->dest_x == 82);
        CHECK(c->src_merge_x == 96);
        CHECK(c->src_and_x == 0);
    }
    CHECK(cracks == 1);
    if (b.w.kind != 2)
        for (const IconPlan& p : pending) CHECK_FALSE(std::holds_alternative<IconOverlayPlan>(p));
    const std::size_t n = pending.size();
    flush_icon_plans();
    CHECK(sink.plans.size() == n);
    CHECK(pending_icon_plans().empty());
    CHECK(sink.all<CrackPlan>().size() == 1u);
}

TEST_CASE("icon effects: the briefing's seg9:0x94ba ices a frozen icon's cell [BYTES seg9:0x94cd-0x9519]") {
    Briefing b;
    Sink sink;
    b.w.added = 0;
    b.w.dir().set_byte(0x1c8 + 1, 2);
    enter_briefing(b.w, b.host);
    CHECK(static_cast<std::int8_t>(b.w.pools.rec(kPoolIcons, 1).flame) == -100);
    CHECK(b.w.dir().byte(0x1c8 + 1) == 2);
    int ice = 0;
    for (const IconPlan& p : pending_icon_plans())
        if (const IconOverlayPlan* o = std::get_if<IconOverlayPlan>(&p))
            if (o->icon == 1 && o->cell == 1) ++ice;
    CHECK(ice == 1);
    clear_icon_plans();
}
