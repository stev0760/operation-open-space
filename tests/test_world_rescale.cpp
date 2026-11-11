// The Size change (src/world/rescale.h; docs/re/shell.md "Rescale (seg10:0x9a2a)", "The image
// rescale (seg10:0x6308)", "Size entry points"): the 16-bit arithmetic of every transition
// 0 <-> 1 <-> 2, the image rescale's calls and slot moves, the object size words, DGROUP:0x740,
// the ship block, the reload path, the beep path that leaves `Quick` 0, the refused path and
// seg10:0x7f88's steps. On Win32 the GDI Blitter rescales a real image. M4 stage 8, SIZE-CORE.
#include <cstdint>
#include <string>
#include <vector>

#include "doctest.h"
#include "world/design_words.h"
#include "world/records.h"
#include "world/rescale.h"
#include "world/sprite.h"
#include "world/world.h"

#ifdef _WIN32
#include "gfx/gfx.h"
#include "gfx/surface.h"
#endif

using namespace is::world;

namespace {

int count(const std::vector<BlitCall>& calls, BlitCall::Op op) {
    int n = 0;
    for (const BlitCall& c : calls) n += c.op == op ? 1 : 0;
    return n;
}
bool deleted(const RecordingBlitter& b, DcId id) {
    for (DcId d : b.deleted)
        if (d == id) return true;
    return false;
}
const RecordingBlitter::Image* image_of(const RecordingBlitter& b, DcId id) {
    for (const auto& i : b.images)
        if (i.id == id) return &i;
    return nullptr;
}

// A host that records what the pass reaches, in order.
struct Host : RescaleHost {
    RecordingBlitter out;
    ImageSlotTable table{};
    std::vector<std::string> log;
    std::int16_t state = 1, enh = 1, dp = 1;
    int reread = 0, beeps = 0, ini = 1;
    bool dialog_ok = true;
    std::vector<std::pair<std::string, int>> ini_writes;
    std::vector<std::uint32_t> posts;
    std::array<DcId, 32> frags{};
    std::int16_t w04 = 0;
    World* world = nullptr;
    // seg14:0x11a8's raw words for a ship without a roster record (the TRANSFORMER: 33, 5).
    std::int16_t raw_side = 33, raw_gap = 5;
    std::vector<int> restored_at_cut;

    Blitter* blitter() override { return &out; }
    ImageSlotTable* images() override { return &table; }
    std::int16_t screen_state() override { return state; }
    std::int16_t enhance() override { return enh; }
    std::int16_t deep() override { return dp; }
    void set_size_reread() override { ++reread; }
    void beep() override { ++beeps; }
    bool open_dialog() override {
        log.push_back("dialog");
        return dialog_ok;
    }
    void close_dialog() override { log.push_back("close"); }
    void post_main(std::uint32_t m) override { posts.push_back(m); }
    void write_ini(const char* key, int v) override { ini_writes.emplace_back(key, v); }
    int ini_size() override { return ini; }
    void todo(const std::string&, const char*) override {}
    DcId* fragment_word(int i) override {
        return i >= 0 && i < static_cast<int>(frags.size()) ? &frags[static_cast<std::size_t>(i)] : nullptr;
    }
    void flame_cells() override { log.push_back("flame_cells"); }
    void ship_sheet(int i) override {
        log.push_back("sheet " + std::to_string(i));
        // seg14:0x11a8 leaves the raw words for +0xd6 = -1 and both wing bytes 1.
        if (world) {
            ObjectRecord& o = world->ship_object(i);
            o.size = raw_side, o.gap = raw_gap;
            set_byte_at(world->ship(i), 0xac, 1);
            set_byte_at(world->ship(i), 0xad, 1);
        }
    }
    void wing_cut(int i, int wing) override {
        log.push_back("cut " + std::to_string(i) + " " + std::to_string(wing));
        if (world) restored_at_cut.push_back(world->restored);
    }
    void gate_images() override { log.push_back("gates"); }
    void logo_cells() override { log.push_back("logo_cells"); }
    void tank_image() override { log.push_back("tank"); }
    void smoke_image() override { log.push_back("smoke"); }
    void logo_strip() override { log.push_back("logo_strip"); }
    void rock_images() override {
        log.push_back("rocks");
        if (world) world->w740 = 0;  // seg10:0x3a2e [BYTES seg10:0x3aa1]
    }
    void effect_images() override { log.push_back("effects"); }
    void pod_image() override { log.push_back("pods"); }
    void hook_sheet() override { log.push_back("hook"); }
    void def_image() override { log.push_back("def"); }
    void race_image() override { log.push_back("race"); }
    void icon_strip() override {
        log.push_back("strip");
        table[0xd] = out.create_image(38 * 8 + 6, 44);
    }
    void icon_overlays() override { log.push_back("overlays"); }
    void slots_freed() override { log.push_back("freed"); }
    void icon_crack(int i) override { log.push_back("crack " + std::to_string(i)); }
    std::int16_t* design_word_04(int) override { return &w04; }
    void barrel_table(int n) override { log.push_back("barrels " + std::to_string(n)); }
    void flame_strip(int i) override { log.push_back("flame " + std::to_string(i)); }

    // An image of w x h in slot k.
    DcId fill(int k, int w, int h) {
        const DcId id = out.create_image(w, h);
        table[static_cast<std::size_t>(k)] = id;
        return id;
    }
    bool logged(const std::string& s) const {
        for (const auto& l : log)
            if (l == s) return true;
        return false;
    }
};

// A wave World: Size 1, pools 0 ... 14 with one record each, the player ship 0.
struct Wave {
    World w;
    Host h;
    Wave() {
        reset_design_words();
        size_session() = SizeSession{};
        w.size = 1, w.shift = 3;
        w.W = 640, w.H = 400;
        for (int p = 0; p < kStartupPools; ++p) w.pools.resize(p, 1);
        w.last_ship = 0;
        w.ships[0].roster_index = -1;
        w.ships[0].design_index = 0;
        set_byte_at(w.ships[0], 0xac, 1);
        set_byte_at(w.ships[0], 0xad, 1);
        h.world = &w;
    }
    ObjectRecord& rec(int p, int r = 0) { return w.pools.rec(p, r); }
};

}  // namespace

// ---- arithmetic

TEST_CASE("rescale: (v << Size) sar 1 in 16 bits, the count masked as a 386 does") {
    CHECK(size_scaled(869, 2) == 1738);
    CHECK(size_scaled(53, 2) == 106);
    CHECK(size_scaled(869, 0) == 434);  // an odd extent loses its last half pixel
    CHECK(size_scaled(53, 0) == 26);
    CHECK(size_scaled(869, 1) == 869);
    CHECK(size_scaled(-5, 0) == -3);   // sar toward minus infinity
    CHECK(size_scaled(5, 33) == 5);    // cl & 0x1f = 1
    CHECK(size_scaled(5, 16) == 0);    // shifted out of the word
    CHECK(size_scaled(0x4000, 2) == 0);  // lo16(0x10000) = 0
    CHECK(rescaled_extent(869, 2) == 1738);
    CHECK(rescaled_extent(473, 0) == 236);
}

TEST_CASE("rescale: the object size words of every transition (seg10:0x9bbc-0x9ca5)") {
    struct Row {
        int pool, delta, new_size;
        std::int16_t enh, deep, size, gap, want_size, want_gap;
    };
    const Row rows[] = {
        // pool 3, a big rock 44/4 and a small rock 22/4 (the templates at Size 1)
        {3, 1, 2, 1, 1, 44, 4, 88, 8},
        {3, 1, 2, 1, 1, 22, 4, 44, 4},   // 44 < 12 << 2: gap 4, the pitch 48 of the 1x big rock
        {3, -1, 1, 1, 1, 44, 4, 22, 2},  // 2 -> 1 at 8 bpp: the halved 2x small rock's pitch 24
        {3, -1, 1, 1, 0, 44, 4, 22, 4},  // 2 -> 1 below 8 bpp with Enhance: gap 4 (the reload)
        {3, -1, 1, 0, 0, 44, 4, 22, 2},  // ... without Enhance
        {3, -1, 0, 1, 1, 22, 4, 11, 2},  // 1 -> 0
        {3, 1, 1, 1, 1, 11, 2, 22, 4},   // 0 -> 1: 22 < 24, gap 4
        {3, 1, 1, 1, 1, 22, 2, 44, 4},   // 0 -> 1 of a big rock at Size 0: 44 >= 24, gap 4 kept
        {3, -1, 0, 1, 1, 44, 8, 22, 4},  // 1 -> 0 of a big rock: no small-rock rule at Size 0
        // the ship pool 14: an odd side at new Size 0 is made even first
        {14, -1, 0, 1, 1, 33, 5, 17, 2},
        {14, 1, 1, 1, 1, 17, 2, 34, 4},
        {14, 1, 2, 1, 1, 33, 5, 66, 10},
        {14, -1, 1, 1, 1, 66, 10, 33, 5},
        // another pool: no rules
        {4, -1, 0, 1, 1, 33, 5, 16, 2},
        {4, 1, 2, 1, 1, 21, 5, 42, 10},
    };
    for (const Row& r : rows) {
        ObjectRecord o{};
        o.size = r.size, o.gap = r.gap, o.extent = 0x123;
        rescale_object_words(o, r.pool, 14, r.delta, r.new_size, r.enh, r.deep);
        INFO("pool " << r.pool << " delta " << r.delta << " size " << r.size);
        CHECK(o.size == r.want_size);
        CHECK(o.gap == r.want_gap);
        CHECK(o.src_y == r.want_gap);  // +0x12 = +0x24
        CHECK(o.extent == 0x123);      // untouched
    }
}

TEST_CASE("rescale: the design words of the tail (seg10:0x9fe0-0xa035)") {
    // TRANSFORMER, side 33: Size 2 gives 8 (the load's 16), Size 0 gives 34 (the load's 17).
    CHECK(rescaled_design(33, 0, 2).w00 == 8);
    CHECK(rescaled_design(33, 0, 2).w02 == 8);
    CHECK(rescaled_design(33, 0, 1).w00 == 16);
    CHECK(rescaled_design(33, 0, 0).w00 == 34);
    CHECK(rescaled_design(32, 0, 0).w00 == 32);
    // +0x04: 4 - w04 - side / 2 only when negative; at Size 0 the raised side.
    CHECK(rescaled_design(33, 3, 1).w04 == 3);
    CHECK(rescaled_design(33, -7, 1).w04 == 4 + 7 - 16);
    CHECK(rescaled_design(33, -7, 0).w04 == 4 + 7 - 17);
    CHECK(rescaled_design(33, -7, 2).w04 == 4 + 7 - 16);
    CHECK(rescaled_design(-3, -1, 1).w04 == 4 + 1 - (-1));  // -3 / 2 = -1 toward zero
}

// ---- seg10:0x6308

TEST_CASE("rescale: an image slot doubled at Size 2 and halved at Size 0") {
    for (const int size : {2, 0}) {
        Host h;
        std::int16_t f740 = 0;
        const DcId old = h.fill(10, 869, 53);
        CHECK(rescale_image(h.out, h.table, 10, kDcNone, -1, size, f740) == kDcNone);
        const DcId now = h.table[10];
        REQUIRE(now != old);
        const auto* img = image_of(h.out, now);
        REQUIRE(img != nullptr);
        CHECK(img->w == (size == 2 ? 1738 : 434));
        CHECK(img->h == (size == 2 ? 106 : 26));
        REQUIRE(h.out.calls.size() == 3);
        CHECK(h.out.calls[0] == pat_blt_call(now, 0, 0, img->w, img->h, kBlackness));
        CHECK(h.out.calls[1].op == BlitCall::kStretchMode);
        CHECK(h.out.calls[1].dst == now);
        CHECK(h.out.calls[1].rop == static_cast<std::uint32_t>(kColorOnColor));
        BlitCall s = bit_blt_call(now, 0, 0, img->w, img->h, old, 0, 0, kSrcCopy);
        s.op = BlitCall::kStretchBlt, s.sw = 869, s.sh = 53;
        CHECK(h.out.calls[2] == s);
        CHECK(deleted(h.out, old));
        CHECK(f740 == 0);
    }
}

TEST_CASE("rescale: slots 6 and 8 copy WHITEONBLACK, every other slot COLORONCOLOR") {
    for (const int k : {6, 8, 7, 9, 0x28}) {
        Host h;
        std::int16_t f740 = 0;
        h.fill(k, 40, 20);
        rescale_image(h.out, h.table, k, kDcNone, -2, 0, f740);
        REQUIRE(count(h.out.calls, BlitCall::kStretchMode) == 1);
        for (const BlitCall& c : h.out.calls)
            if (c.op == BlitCall::kStretchMode)
                CHECK(c.rop == static_cast<std::uint32_t>(k == 6 || k == 8 ? kWhiteOnBlack : kColorOnColor));
    }
}

TEST_CASE("rescale: at Size 2 slots 6 and 7 move to 8 and 9 and DGROUP:0x740 is set") {
    Host h;
    std::int16_t f740 = 0;
    const DcId big_mask = h.fill(6, 869, 53), big = h.fill(7, 869, 53);
    const DcId small_mask = h.fill(8, 473, 31), small = h.fill(9, 473, 31);
    for (int k = 6; k <= 9; ++k) rescale_image(h.out, h.table, k, kDcNone, -2, 2, f740);
    CHECK(f740 == 1);
    // 8 and 9 hold the unscaled big rocks, 6 and 7 their doubles.
    CHECK(h.table[8] == big_mask);
    CHECK(h.table[9] == big);
    CHECK(deleted(h.out, small_mask));
    CHECK(deleted(h.out, small));
    CHECK(!deleted(h.out, big_mask));
    CHECK(!deleted(h.out, big));
    for (int k : {6, 7}) {
        const auto* img = image_of(h.out, h.table[static_cast<std::size_t>(k)]);
        REQUIRE(img != nullptr);
        CHECK(img->w == 1738);
        CHECK(img->h == 106);
    }
    CHECK(count(h.out.calls, BlitCall::kStretchBlt) == 2);  // 8 and 9 were left alone
    // Halving back (2 -> 1 without the reload): 8 and 9 become the 1x big rocks halved; the flag stays.
    for (int k = 6; k <= 9; ++k) rescale_image(h.out, h.table, k, kDcNone, -2, 0, f740);
    CHECK(f740 == 1);
    const auto* s9 = image_of(h.out, h.table[9]);
    REQUIRE(s9 != nullptr);
    CHECK(s9->w == 434);
    CHECK(s9->h == 26);
    CHECK(deleted(h.out, big));
}

TEST_CASE("rescale: Size 1 returns at once unless mode 2 forces it; an empty slot returns 0") {
    Host h;
    std::int16_t f740 = 0;
    const DcId a = h.fill(4, 30, 20);
    CHECK(rescale_image(h.out, h.table, 4, 0x77, -1, 1, f740) == 0x77);  // hbm back, untouched
    CHECK(h.table[4] == a);
    CHECK(h.out.calls.empty());
    CHECK(rescale_image(h.out, h.table, 4, kDcNone, kRescaleModeForce, 1, f740) == kDcNone);
    CHECK(h.table[4] != a);
    const auto* img = image_of(h.out, h.table[4]);
    REQUIRE(img != nullptr);
    CHECK(img->w == 30);  // (w << 1) sar 1
    CHECK(img->h == 20);
    h.out.calls.clear();
    CHECK(rescale_image(h.out, h.table, 5, kDcNone, -2, 2, f740) == kDcNone);  // empty slot
    CHECK(h.out.calls.empty());
    CHECK(h.table[5] == kDcNone);
    CHECK(rescale_image(h.out, h.table, kImageSlots, kDcNone, -2, 2, f740) == kDcNone);  // outside the table
}

TEST_CASE("rescale: the bitmap form returns the new image and deletes the old") {
    Host h;
    std::int16_t f740 = 0;
    const DcId piece = h.out.create_image(32, 32);
    const DcId got = rescale_image(h.out, h.table, kRescaleSlotNone, piece, -1, 2, f740);
    REQUIRE(got != kDcNone);
    REQUIRE(got != piece);
    CHECK(deleted(h.out, piece));
    const auto* img = image_of(h.out, got);
    REQUIRE(img != nullptr);
    CHECK(img->w == 64);
    CHECK(img->h == 64);
    for (const BlitCall& c : h.out.calls)
        if (c.op == BlitCall::kStretchMode) CHECK(c.rop == static_cast<std::uint32_t>(kColorOnColor));
    // A loaded bitmap whose size the Blitter does not know is handed back unchanged [DESIGN ADR 0016].
    const DcId loaded = h.out.load_bitmap("OUT_SIGN");
    CHECK(rescale_image(h.out, h.table, kRescaleSlotNone, loaded, -1, 0, f740) == loaded);
    h.out.set_image_size(loaded, 40, 21);
    const DcId half = rescale_image(h.out, h.table, kRescaleSlotNone, loaded, -1, 0, f740);
    REQUIRE(half != loaded);
    CHECK(image_of(h.out, half)->w == 20);
    CHECK(image_of(h.out, half)->h == 10);
    CHECK(f740 == 0);
}

TEST_CASE("rescale: free_image_slot is seg10:0x552(slot, 1)") {
    Host h;
    const DcId a = h.fill(4, 8, 8);
    free_image_slot(h.out, h.table, 4);
    CHECK(h.table[4] == kDcNone);
    CHECK(deleted(h.out, a));
    const std::size_t n = h.out.deleted.size();
    free_image_slot(h.out, h.table, 4);  // empty: nothing
    free_image_slot(h.out, h.table, -1);
    free_image_slot(h.out, h.table, kImageSlots);
    CHECK(h.out.deleted.size() == n);
}

// ---- seg10:0x9a2a

TEST_CASE("rescale step 1 -> 2: images, size words, ship block, INI, dialog, black, Quick") {
    Wave v;
    load_quick(1);
    v.h.fill(7, 869, 53);
    v.h.fill(6, 869, 53);
    v.h.fill(8, 473, 31);
    v.h.fill(9, 473, 31);
    const DcId backup = v.h.fill(kSlotIconBackup, 256, 32);
    v.h.fill(0xd, 310, 44);
    v.h.fill(0x28, 640, 140);
    v.h.frags[0] = v.h.out.create_image(10, 10);
    v.w.pools.set_in_use(1, 1);
    v.rec(3).size = 22, v.rec(3).gap = 4;  // a small rock
    v.rec(14).size = 33, v.rec(14).gap = 5;
    v.rec(14).extent = 33 << 3;
    set_byte_at(v.w.ships[0], 0xac, 0);  // a lost wing
    DesignWordSet dw;
    dw.w2a = 33;
    set_design_words(0, dw);

    rescale_step(v.w, 1, v.h);

    CHECK(v.w.size == 2);
    CHECK(v.w.shift == 2);
    CHECK(quick_word() == 1);  // restored
    CHECK(v.w.w740 == 1);
    CHECK(v.h.table[kSlotIconBackup] == backup);  // never rescaled
    CHECK(image_of(v.h.out, v.h.table[0xd])->w == 620);
    CHECK(image_of(v.h.out, v.h.frags[0])->w == 20);  // the fragment doubled
    CHECK(v.rec(3).size == 44);
    CHECK(v.rec(3).gap == 4);
    // The ship block ran (Size 2 with Enhance): slot 0x28 freed, the sheet, then 66/10.
    CHECK(v.h.logged("sheet 0"));
    CHECK(v.rec(14).size == 66);
    CHECK(v.rec(14).gap == 10);
    CHECK(v.rec(14).src_y == 10);
    CHECK(v.rec(14).extent == 33 << 3);  // the ship's extent is untouched
    CHECK(design_words(0).w2a == 33);
    // The lost wing is cut again with DGROUP:0x92e4 = 1, which is 0 afterwards.
    CHECK(v.h.logged("cut 0 0"));
    CHECK(!v.h.logged("cut 0 1"));
    REQUIRE(v.h.restored_at_cut.size() == 1);
    CHECK(v.h.restored_at_cut[0] == 1);
    CHECK(v.w.restored == 0);
    // No reload path.
    CHECK(!v.h.logged("gates"));
    CHECK(!v.h.logged("strip"));
    // The tail: design words at Size 2, the barrels and the flame strip.
    CHECK(design_words(0).w00 == 8);
    CHECK(design_words(0).w02 == 8);
    CHECK(v.h.logged("barrels 0"));
    CHECK(v.h.logged("flame 0"));
    CHECK(v.h.logged("flame_cells"));
    // INI, the dialog, the post and BLACKNESS on the play area.
    REQUIRE(v.h.ini_writes.size() == 2);
    CHECK(v.h.ini_writes[0] == std::make_pair(std::string("Size"), 2));
    CHECK(v.h.ini_writes[1] == std::make_pair(std::string("SetSize"), 2));
    CHECK(v.h.logged("dialog"));
    CHECK(v.h.logged("close"));
    REQUIRE(v.h.posts.size() == 1);
    CHECK(v.h.posts[0] == 0x978u);
    CHECK(size_session().dialog == 0);
    CHECK(size_session().dg_9518 == 0);
    CHECK(v.h.out.calls.back() == pat_blt_call(kDcWindow, 0, 0, 640, 400, kBlackness));
    CHECK(size_session().dg_103a == 0);
}

TEST_CASE("rescale step 1 -> 2 without Enhance keeps the doubled sheets") {
    Wave v;
    v.h.enh = 0;
    const DcId sheet = v.h.fill(0x28, 640, 140);
    v.rec(14).size = 33, v.rec(14).gap = 5;
    rescale_step(v.w, 1, v.h);
    CHECK(!v.h.logged("sheet 0"));
    CHECK(v.h.table[0x28] != sheet);
    CHECK(image_of(v.h.out, v.h.table[0x28])->w == 1280);
    CHECK(v.rec(14).size == 66);
    CHECK(v.rec(14).gap == 10);
}

TEST_CASE("rescale step 2 -> 1 at 8 bpp halves everything, no ship block, DGROUP:0x740 stays") {
    Wave v;
    v.w.size = 2, v.w.shift = 2, v.w.w740 = 1;
    v.h.fill(0x28, 1280, 280);
    v.rec(3).size = 44, v.rec(3).gap = 4;
    rescale_step(v.w, -1, v.h);
    CHECK(v.w.size == 1);
    CHECK(v.w.shift == 3);
    CHECK(v.w.w740 == 1);
    CHECK(image_of(v.h.out, v.h.table[0x28])->w == 640);
    CHECK(v.rec(3).size == 22);
    CHECK(v.rec(3).gap == 2);
    CHECK(!v.h.logged("sheet 0"));
    CHECK(!v.h.logged("gates"));
    CHECK(v.h.ini_writes[0] == std::make_pair(std::string("Size"), 1));
}

TEST_CASE("rescale step 2 -> 1 below 8 bpp with Enhance takes the reload path") {
    Wave v;
    v.w.size = 2, v.w.shift = 2, v.w.w740 = 1;
    v.h.dp = 0;
    v.w.pools.resize(0, 2);
    const DcId flame0 = v.h.fill(14, 64, 64);
    const DcId other = v.h.fill(10, 40, 40);
    v.h.fill(kSlotIconBackup, 64, 32);
    v.h.fill(0xd, 2 * 38 + 6, 44);
    v.rec(3).size = 44, v.rec(3).gap = 4;
    rescale_step(v.w, -1, v.h);
    CHECK(size_session().dg_103a == 1);
    CHECK(v.h.table[14] != flame0);  // the icon flame images are rescaled
    CHECK(image_of(v.h.out, v.h.table[14])->w == 32);
    // The other slots are not rescaled; the reload frees them and calls the builders.
    CHECK(deleted(v.h.out, other));
    CHECK(v.h.table[10] == kDcNone);
    CHECK(v.rec(3).size == 22);
    CHECK(v.rec(3).gap == 4);
    CHECK(v.w.w740 == 0);  // seg10:0x3a2e cleared it
    // The ship block and the builders in the bytes' order.
    const std::vector<std::string> want = {"gates", "logo_cells", "tank",  "smoke", "logo_strip",
                                           "rocks", "effects",    "pods",  "hook",  "def",
                                           "strip", "overlays",   "crack 0", "crack 1"};
    std::vector<std::string> got;
    for (const auto& l : v.h.log)
        for (const auto& x : want)
            if (l == x) got.push_back(l);
    CHECK(got == want);
    CHECK(v.h.logged("sheet 0"));
    // The cells of the backup into the fresh strip at (38i + 6, 6).
    int cells = 0;
    for (const BlitCall& c : v.h.out.calls)
        if (c.op == BlitCall::kBitBlt && c.dst == v.h.table[0xd]) {
            CHECK(c.src == v.h.table[kSlotIconBackup]);
            CHECK(c.x == 38 * cells + 6);
            CHECK(c.y == 6);
            CHECK(c.w == 32);
            CHECK(c.sx == 32 * cells);
            ++cells;
        }
    CHECK(cells == 2);
}

TEST_CASE("rescale step 1 -> 0 and 0 -> 1") {
    Wave v;
    v.h.fill(10, 40, 40);
    v.h.fill(0x28, 640, 140);
    v.rec(14).size = 33, v.rec(14).gap = 5;
    v.rec(3).size = 22, v.rec(3).gap = 4;
    DesignWordSet dw;
    dw.w2a = 33;
    set_design_words(0, dw);
    rescale_step(v.w, -1, v.h);
    CHECK(v.w.size == 0);
    CHECK(v.w.shift == 4);
    CHECK(v.rec(14).size == 17);
    CHECK(v.rec(14).gap == 2);
    CHECK(v.rec(3).size == 11);
    CHECK(v.rec(3).gap == 2);
    CHECK(image_of(v.h.out, v.h.table[10])->w == 20);
    CHECK(!v.h.logged("sheet 0"));
    CHECK(design_words(0).w00 == 34);
    // 0 -> 1 is a reload: 22/4 rocks, the sheet re-rendered at 1x (the raw 33/5).
    v.h.log.clear();
    rescale_step(v.w, 1, v.h);
    CHECK(v.w.size == 1);
    CHECK(v.rec(3).size == 22);
    CHECK(v.rec(3).gap == 4);
    CHECK(v.h.logged("sheet 0"));
    CHECK(v.rec(14).size == 33);
    CHECK(v.rec(14).gap == 5);
    CHECK(v.h.logged("gates"));
    CHECK(design_words(0).w00 == 16);
}

TEST_CASE("rescale step: the side and gap come from the roster record when the ship has one") {
    Wave v;
    v.w.roster.resize(1);
    v.w.roster[0].size = 30;
    v.w.roster[0].gap = 6;
    v.w.ships[0].roster_index = 0;
    rescale_step(v.w, 1, v.h);
    CHECK(design_words(0).w2a == 30);
    CHECK(v.rec(14).size == 60);
    CHECK(v.rec(14).gap == 12);
}

TEST_CASE("rescale step: the beep path leaves Quick 0 and changes nothing else") {
    Wave v;
    v.w.size = 2, v.w.shift = 2;
    load_quick(1);
    v.rec(3).size = 44, v.rec(3).gap = 4;
    v.h.fill(10, 40, 40);
    rescale_step(v.w, 1, v.h);
    CHECK(v.h.beeps == 1);
    CHECK(quick_word() == 0);  // the original's fault, kept (ruling R1)
    CHECK(v.w.size == 2);
    CHECK(v.rec(3).size == 44);
    CHECK(v.h.ini_writes.empty());
    CHECK(v.h.out.calls.empty());
    CHECK(v.h.log.empty());
    // At Size 0, Ctrl - beeps too.
    v.w.size = 0;
    rescale_step(v.w, -1, v.h);
    CHECK(v.h.beeps == 2);
}

TEST_CASE("rescale step: outside the wave screen or in seg18 mode only DGROUP:0x103c is set") {
    Wave v;
    load_quick(1);
    v.h.state = 2;
    rescale_step(v.w, 1, v.h);
    CHECK(v.h.reread == 1);
    CHECK(quick_word() == 1);  // the gate comes before the save
    CHECK(v.w.size == 1);
    CHECK(v.h.ini_writes.empty());
    v.h.state = 1;
    v.w.w9130 = 1;
    rescale_step(v.w, -1, v.h);
    CHECK(v.h.reread == 2);
    CHECK(v.w.size == 1);
}

TEST_CASE("rescale step: a demo writes Size only; a dialog that fails is not closed") {
    Wave v;
    v.w.player_game = 0;
    v.h.dialog_ok = false;
    rescale_step(v.w, 1, v.h);
    REQUIRE(v.h.ini_writes.size() == 1);
    CHECK(v.h.ini_writes[0].first == "Size");
    CHECK(!v.h.logged("close"));
    CHECK(v.h.posts.empty());
    CHECK(size_session().dg_9518 == 1);  // set before CreateDialog, cleared only on the close path
}

TEST_CASE("rescale step: the pool-8 kind-2 entries in the ship block") {
    Wave v;
    v.w.pools.resize(8, 3);
    v.w.pools.set_in_use(8, 3);
    // entry 0: class 1 -> kind 1; entry 1: class 3, left; entry 2: class 5, the size words
    v.w.pool8[0].at(0x1c) = 2, v.w.pool8[0].at(0x30) = 1;
    v.w.pool8[1].at(0x1c) = 2, v.w.pool8[1].at(0x30) = 3;
    v.w.pool8[2].at(0x1c) = 2, v.w.pool8[2].at(0x30) = 5;
    v.w.pool8[2].at(0x1e) = 0x55;
    v.w.classes[5][8] = 40;  // side
    v.w.classes[5][9] = 6;   // gap
    v.rec(8, 1).extent = 0x111;
    rescale_step(v.w, 1, v.h);
    CHECK(v.w.pool8[0].at(0x1c) == 1);
    CHECK(v.w.pool8[1].at(0x1c) == 2);
    CHECK(v.w.pool8[2].at(0x1c) == 2);
    CHECK(v.rec(8, 2).size == 80);
    CHECK(v.rec(8, 2).gap == 12);
    CHECK(v.rec(8, 2).extent == 80 << 3);  // the scaled side << 3 (the original's fault, kept)
    CHECK(v.rec(8, 1).extent == 0x111);
    CHECK(v.w.pool8[2].at(0x1e) == 0x55);  // the M5 stub keeps the word
}

TEST_CASE("rescale step: a Demon's Gate wave leaves pool 0's size words") {
    Wave v;
    v.w.roles.radar_first = 1;
    v.rec(0).size = 32, v.rec(0).gap = 6;
    v.rec(1).size = 10, v.rec(1).gap = 2;
    rescale_step(v.w, 1, v.h);
    CHECK(v.rec(0).size == 32);
    CHECK(v.rec(1).size == 20);
}

// ---- seg10:0x7f88

TEST_CASE("size reread: the INI against Size, one or two steps") {
    SUBCASE("no difference: nothing") {
        Wave v;
        v.h.fill(0x28, 640, 140);
        v.h.ini = 1;
        size_reread(v.w, v.h);
        CHECK(v.h.ini_writes.empty());
        CHECK(v.h.reread == 0);
    }
    SUBCASE("no ship sheet: DGROUP:0x92e2 and DGROUP:0x103c") {
        Wave v;
        v.h.ini = 2;
        size_reread(v.w, v.h);
        CHECK(size_session().dg_92e2 == 1);
        CHECK(v.h.reread == 1);
        CHECK(v.w.size == 1);
    }
    SUBCASE("a difference of one") {
        Wave v;
        v.h.fill(0x28, 640, 140);
        v.h.ini = 0;
        size_reread(v.w, v.h);
        CHECK(v.w.size == 0);
        CHECK(v.h.ini_writes.size() == 2);
    }
    SUBCASE("a difference of two: two steps") {
        Wave v;
        v.w.size = 0, v.w.shift = 4;
        v.h.fill(0x28, 320, 70);
        v.h.ini = 2;
        size_reread(v.w, v.h);
        CHECK(v.w.size == 2);
        CHECK(v.h.beeps == 0);
    }
    SUBCASE("an INI Size outside 0 ... 2: the second step beeps and leaves Quick 0 (ruling R2)") {
        Wave v;
        load_quick(1);
        v.h.fill(0x28, 640, 140);
        v.h.ini = 5;
        size_reread(v.w, v.h);
        CHECK(v.w.size == 2);
        CHECK(v.h.beeps == 1);
        CHECK(quick_word() == 0);
    }
    SUBCASE("the INI value a refused read stood Size 1 in for: no difference (ruling R2, review M3)") {
        Wave v;
        load_quick(1);
        v.h.fill(0x28, 640, 140);
        CHECK(size_from_ini(3, "test") == 1);  // the start-up or the load refused INI `Size` 3
        CHECK(size_session().refused);
        v.h.ini = 3;
        size_reread(v.w, v.h);  // the original's DGROUP:0x9156 holds 3: no step, no write, no beep
        CHECK(v.w.size == 1);
        CHECK(v.h.ini_writes.empty());
        CHECK(v.h.beeps == 0);
        CHECK(v.h.reread == 0);
        CHECK(quick_word() == 1);
        // Another value compares with the port's Size as the bytes compare it, and the completed
        // step ends the stand-in.
        v.h.ini = 2;
        size_reread(v.w, v.h);
        CHECK(v.w.size == 2);
        CHECK_FALSE(size_session().refused);
        CHECK(size_from_ini(0, "test") == 0);
        CHECK(size_from_ini(-1, "test") == 1);
        CHECK(size_session().refused_ini == -1);
        CHECK(size_from_ini(2, "test") == 2);
        CHECK_FALSE(size_session().refused);
    }
}

// ---- seg9:0x23e4-0x2418 and seg9:0xd38-0xd5c

TEST_CASE("load reread: DGROUP:0x92e2 rereads INI Size and frees every slot (review H1)") {
    SUBCASE("the flag clear: the Size in force, the table untouched") {
        Wave v;
        const DcId id = v.h.fill(6, 40, 40);
        v.h.ini = 2;
        load_size_reread(v.w, v.h);
        CHECK(v.w.size == 1);
        CHECK(v.w.shift == 3);
        CHECK(v.h.table[6] == id);
        CHECK(v.h.log.empty());
        CHECK(size_session().dg_92e2 == 0);
    }
    SUBCASE("the flag set: INI Size, DGROUP:0x9150, seg10:0x522(1), seg10:0x5c42, the flag cleared") {
        Wave v;
        size_session().dg_92e2 = 1;
        const DcId a = v.h.fill(0, 4, 4), b = v.h.fill(0x28, 640, 140), c = v.h.fill(0x39, 8, 8);
        v.h.ini = 2;
        load_size_reread(v.w, v.h);
        CHECK(v.w.size == 2);
        CHECK(v.w.shift == 2);
        for (std::size_t k = 0; k < 0x3a; ++k) CHECK(v.h.table[k] == kDcNone);
        CHECK(deleted(v.h.out, a));
        CHECK(deleted(v.h.out, b));
        CHECK(deleted(v.h.out, c));
        CHECK(count(v.h.out.calls, BlitCall::Op::kStretchBlt) == 0);  // nothing is rescaled
        REQUIRE(v.h.log.size() == 2);
        CHECK(v.h.log[0] == "freed");
        CHECK(v.h.log[1] == "flame_cells");
        CHECK(size_session().dg_92e2 == 0);
    }
    SUBCASE("an INI Size of 3: Size 1 stands in for it, and seg10:0x7f88 then finds no difference") {
        Wave v;
        size_session().dg_92e2 = 1;
        v.h.ini = 3;
        load_size_reread(v.w, v.h);
        CHECK(v.w.size == 1);
        CHECK(v.w.shift == 3);
        v.h.fill(0x28, 640, 140);
        size_reread(v.w, v.h);
        CHECK(v.h.ini_writes.empty());
        CHECK(v.h.beeps == 0);
    }
}

TEST_CASE("new game logo: seg9:0xc06 makes slot 5 again at a Size other than 1 (review H2)") {
    SUBCASE("Size 1: nothing") {
        Wave v;
        const DcId id = v.h.fill(5, 650, 52);
        v.h.fill(0x16, 10, 10);
        new_game_logo(v.w, v.h);
        CHECK(v.h.table[5] == id);
        CHECK(v.h.table[0x16] != kDcNone);
        CHECK(v.h.log.empty());
    }
    for (int size : {0, 2}) {
        CAPTURE(size);
        Wave v;
        v.w.size = static_cast<std::int16_t>(size), v.w.shift = static_cast<std::int16_t>(4 - size);
        const DcId logo = v.h.fill(5, 650, 52), cells = v.h.fill(0x16, 10, 10);
        new_game_logo(v.w, v.h);
        CHECK(deleted(v.h.out, logo));  // seg10:0x552(5, 1) before seg10:0x39a8(1)
        CHECK(v.h.logged("logo_strip"));
        CHECK(deleted(v.h.out, cells));  // seg10:0x552(0x16, 1)
        CHECK(v.h.table[0x16] == kDcNone);
    }
}

TEST_CASE("rescale: the session word Quick") {
    size_session() = SizeSession{};
    load_quick(2);
    CHECK(quick_word() == 2);
    load_quick(0x10001);  // a 16-bit word
    CHECK(quick_word() == 1);
}

#ifdef _WIN32
// The GDI Blitter measures and rescales a real index image: a doubling duplicates every pixel, a
// WHITEONBLACK halving keeps every set pixel of a mask, and a COLORONCOLOR halving keeps one pixel
// of each pair, whichever GDI chooses (ruling R4) [DESIGN ADR 0017].
TEST_CASE("rescale: the GDI blitter on a real image") {
    is::gfx::Gfx gfx(nullptr);
    gfx.set_client_size(64, 48, 1);
    REQUIRE(gfx.logical_width() > 0);
    GdiBlitter b(gfx);
    ImageSlotTable table{};
    std::int16_t f740 = 0;
    // 4 x 2: columns 10, 20, 30, 40 in both rows.
    const std::vector<std::uint8_t> px = {10, 20, 30, 40, 10, 20, 30, 40};
    table[10] = b.create_image_from(4, 2, px);
    REQUIRE(table[10] != kDcNone);
    int w = 0, h = 0;
    REQUIRE(b.image_size(table[10], w, h));
    CHECK(w == 4);
    CHECK(h == 2);
    rescale_image(b, table, 10, kDcNone, -2, 2, f740);
    REQUIRE(b.image_size(table[10], w, h));
    CHECK(w == 8);
    CHECK(h == 4);
    auto img = b.image(table[10]);
    img->flush();
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 8; ++x) CHECK(img->at(x, y) == px[static_cast<std::size_t>(x / 2)]);
    // Halved twice back to 2 x 1 with COLORONCOLOR: each pixel is one of its pair's.
    rescale_image(b, table, 10, kDcNone, -2, 0, f740);
    rescale_image(b, table, 10, kDcNone, -2, 0, f740);
    REQUIRE(b.image_size(table[10], w, h));
    CHECK(w == 2);
    CHECK(h == 1);
    img = b.image(table[10]);
    img->flush();
    CHECK((img->at(0, 0) == 10 || img->at(0, 0) == 20));
    CHECK((img->at(1, 0) == 30 || img->at(1, 0) == 40));
    // A mask in slot 6 (255 set, 0 clear): WHITEONBLACK ORs the pixels a halving drops, so a set
    // pixel at an odd column survives.
    const std::vector<std::uint8_t> mask = {0, 255, 0, 255, 0, 0, 255, 0, 0, 255, 0, 255, 0, 0, 255, 0};
    table[6] = b.create_image_from(8, 2, mask);
    rescale_image(b, table, 6, kDcNone, -2, 0, f740);
    REQUIRE(b.image_size(table[6], w, h));
    CHECK(w == 4);
    CHECK(h == 1);
    img = b.image(table[6]);
    img->flush();
    CHECK(img->at(0, 0) == 255);
    CHECK(img->at(1, 0) == 255);
    CHECK(img->at(2, 0) == 0);
    CHECK(img->at(3, 0) == 255);
    // A 1-pixel extent halves to 0, which makes a 1 x 1 bitmap.
    table[11] = b.create_image_from(8, 1, std::vector<std::uint8_t>(8, 7));
    rescale_image(b, table, 11, kDcNone, -2, 0, f740);
    REQUIRE(b.image_size(table[11], w, h));
    CHECK(w == 1);
    CHECK(h == 1);
    CHECK(f740 == 0);
}

// stretch_index is GDI's StretchBlt under Wine, pixel for pixel: the same black image, mode and
// copy made through the GDI Blitter's calls (seg10:0x6308's PatBlt, SetStretchBltMode and
// seg10:0x9962) against the index walk the port runs on every host, for COLORONCOLOR and
// WHITEONBLACK, halving and doubling (ruling R4, review M1) [DESIGN ADR 0017 decision 2].
TEST_CASE("rescale: stretch_index equals GDI's StretchBlt (Win32)") {
    is::gfx::Gfx gfx(nullptr);
    gfx.set_client_size(64, 48, 1);
    REQUIRE(gfx.logical_width() > 0);
    GdiBlitter b(gfx);
    const std::pair<int, int> sizes[] = {{8, 4}, {13, 7}, {31, 31}, {2, 3}, {3, 1}, {1, 5}, {2, 2}, {3, 3},
                                         {2, 5}, {5, 2}, {3, 4}, {650, 52}, {98, 61}};
    for (const auto& sz : sizes)
        for (int level : {0, 2})
            for (int mode : {kWhiteOnBlack, kColorOnColor})
                for (int pattern : {0, 1}) {
                    const int w = sz.first, h = sz.second;
                    const int W = size_scaled(static_cast<std::int16_t>(w), level);
                    const int H = size_scaled(static_cast<std::int16_t>(h), level);
                    if (W <= 0 || H <= 0) continue;
                    CAPTURE(w);
                    CAPTURE(h);
                    CAPTURE(level);
                    CAPTURE(mode);
                    CAPTURE(pattern);
                    std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x) {
                            const int k = y * w + x;
                            // 0: colour indices 1 ... 250; 1: a mask of 0 and 255 with lone set pixels.
                            px[static_cast<std::size_t>(k)] =
                                pattern == 0 ? static_cast<std::uint8_t>(1 + (x * 7 + y * 13) % 250)
                                             : static_cast<std::uint8_t>(((x * 5 + y * 3) % 7) == 0 ? 255 : 0);
                        }
                    const DcId src = b.create_image_from(w, h, px);
                    const DcId dst = b.create_image(W, H);
                    REQUIRE(src != kDcNone);
                    REQUIRE(dst != kDcNone);
                    b.pat_blt(dst, 0, 0, W, H, kBlackness);
                    b.set_stretch_mode(dst, mode);
                    b.stretch_blt(dst, 0, 0, W, H, src, 0, 0, w, h, kSrcCopy);
                    std::vector<std::uint8_t> got;
                    int gw = 0, gh = 0;
                    REQUIRE(b.read_index(dst, got, gw, gh));
                    REQUIRE(gw == W);
                    REQUIRE(gh == H);
                    const std::vector<std::uint8_t> want = stretch_index(px, w, h, W, H, mode);
                    int diff = 0;
                    for (std::size_t k = 0; k < want.size(); ++k) diff += got[k] != want[k];
                    CHECK(diff == 0);
                    b.delete_image(src);
                    b.delete_image(dst);
                }
}

#endif
