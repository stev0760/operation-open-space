// WILDCARD DEFENSE (wildcard.h). Ported from the listing of seg11:0x9078 (isre asm, INSPACE.EXE
// seg11) with work/analysis/m4/s7-fuel.md section 5 and s7-fuel-verify.md. M4 stage 7, track
// FUEL.
#include "world/wildcard.h"

#include <cstddef>

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

// `shl r, cl` with cl the byte DGROUP:0x9156: the count is taken mod 32.
int shift_of(std::int16_t v) { return v & 0x1f; }

// The bitmap's gap, `test byte [bp-0x18], 1`: 5 for an odd bmHeight, else 6 [BYTES
// seg11:0x923b-0x9255].
std::int16_t raw_gap(int h) { return (h & 1) != 0 ? std::int16_t{5} : std::int16_t{6}; }

}  // namespace

void WildcardHost::erase(int x, int y, int w, int h) {
    if (Blitter* out = wave_blitter()) out->pat_blt(kDcWindow, x, y, w, h, kBlackness);
}

std::int16_t WildcardHost::fruit_image(int id) {
    static bool once = false;
    guard_todo(once, "the fruit picture of slot 0x39 without a drawing side", "seg11:0x9146");
    const int k = id - kFruitGraphic;
    return k >= 0 && k < static_cast<int>(kFruitHeights.size()) ? kFruitHeights[ix(k)] : std::int16_t{0};
}

WildcardLinks& wildcard_links() {
    static WildcardLinks l;
    return l;
}

namespace {

class LinkedWildcardHost final : public WildcardHost {
public:
    void erase(int x, int y, int w, int h) override {
        if (wildcard_links().erase) return wildcard_links().erase(x, y, w, h);
        WildcardHost::erase(x, y, w, h);
    }
    std::int16_t fruit_image(int id) override {
        if (wildcard_links().fruit_image) return wildcard_links().fruit_image(id);
        return WildcardHost::fruit_image(id);
    }
};

}  // namespace

WildcardHost& linked_wildcard_host() {
    static LinkedWildcardHost h;
    return h;
}

// ---------------------------------------------------------------- seg11:0x9078

void wildcard(World& w, const WaveGlobals& g, WeaponWords& W, shell::Rng& rng, WildcardHost& h,
              int pool, int rec) {
    Pools& p = w.pools;
    bool go = false;  // [bp-0x28] [BYTES seg11:0x9087]
    if (pool == g.ship_pool) {  // [BYTES seg11:0x908c-0x9092]
        ShipRecord& S = ship_at(w, rec);
        const int n = static_cast<std::int8_t>(S.slots_cb());  // (int8) +0xcb, cbw [BYTES seg11:0x90a3-0x90ab]
        for (int l = 1; l < n; ++l) {
            const int b = static_cast<std::uint8_t>(sbyte_at(S, 0xb8 + l));  // `cmp byte` [BYTES seg11:0x90b2-0x90be]
            if (b == kWeaponWildcard || b == kWeaponShock) {
                go = true;
                break;
            }
        }
    } else {
        go = true;  // [BYTES seg11:0x9092 seg11:0x90c0]
    }
    if (!go) return;  // [BYTES seg11:0x90c5-0x90c9]
    const int S = shift_of(w.size);  // DGROUP:0x9156
    // m < DGROUP:0x9412, re-read [BYTES seg11:0x93ce-0x93d7]
    for (int m = 0; m < p.in_use(kPoolMissiles); ++m) {
        if (m >= kMissileSlots || m >= p.records(kPoolMissiles)) break;  // [DESIGN] the tables' bound
        MissileSlot& E = p.missile(m);
        // The tests [BYTES seg11:0x93db-0x9439] (verified claim 37). A free entry's weapon -1
        // reads W(-1)+0x1e = 0, as the original's BSS word DGROUP:0x2f22 does (O10).
        if (E.age <= 0) continue;
        if (!(W.word(E.weapon, 0x1e) > E.age)) continue;
        if (E.weapon == kWeaponFruit) continue;
        {
            const ObjectRecord& U = obj(w, pool, rec);
            const ObjectRecord& M = p.rec(kPoolMissiles, m);
            std::int16_t dx = 0, dy = 0;
            // seg8:0x285a(&[bp-6], &[bp-8], user +4, missile +4), Pascal: (x1, y1) is the last
            // pushed point, the missile, as in missile.cpp's retarget [BYTES seg11:0x93fc-0x942e
            // seg12:0x680-0x6b0]; the result's low word, signed [BYTES seg11:0x9433-0x9439].
            if (distance_cells(M.x, M.y, U.x, U.y, dx, dy) >= kWildcardRange) continue;
        }
        // 3. The conversion. The square with no margin, whatever `on_screen` says (O10) [BYTES
        // seg11:0x943b-0x9458].
        {
            const ObjectRecord& M = p.rec(kPoolMissiles, m);
            h.erase(M.scr_x, M.scr_y, M.size, M.size);
        }
        // [bp-0xc] = the old weapon, never read [BYTES seg11:0x945d-0x9461]. The live fruit e:
        // the last k < DGROUP:0x9412 with age > 0, below its lifetime and weapon 27 [BYTES
        // seg11:0x9464-0x9105] (verified claim 41).
        int e = -1;
        for (int k = 0; k < p.in_use(kPoolMissiles) && k < kMissileSlots; ++k) {
            const MissileSlot& K = p.missile(k);
            if (K.age > 0 && W.word(K.weapon, 0x1e) > K.age && K.weapon == kWeaponFruit) e = k;
        }
        E.weapon = kWeaponFruit;                        // [BYTES seg11:0x910b]
        E.age = 1;                                      // [BYTES seg11:0x9111]
        E.steer_delay = W.word(kWeaponFruit, 0x1e);     // DGROUP:0x37e2 [BYTES seg11:0x9117-0x911a]
        W.set_word(kWeaponFruit, -4, kFruitSlot);       // DGROUP:0x37c0 = 0x39 [BYTES seg11:0x911e]
        W.set_word(kWeaponFruit, 0x24, 5);              // DGROUP:0x37e8 = 5 [BYTES seg11:0x9124]
        p.rec(kPoolMissiles, m).on_destroy.set(kDestroyPool11);  // far seg12:0x6a60 [BYTES seg11:0x912a-0x9138]
        if (e == -1) {
            // 4. The fruit picture [BYTES seg11:0x9146-0x93ac] (verified claims 39, 40)
            const int id = kFruitGraphic + imod16(lo16(rng.rand()), 3);  // seg5:0xdc; cwd; idiv 3 [BYTES seg11:0x91b0-0x91bf]
            const std::int16_t hgt = h.fruit_image(id);
            ObjectRecord& F = p.rec(kPoolMissiles, m);
            F.size = hgt;                                // bmHeight [BYTES seg11:0x922c-0x9237]
            F.gap = raw_gap(hgt);                        // [BYTES seg11:0x923b-0x9255]
            F.extent = shl16(F.size, 3);                 // the unscaled height [BYTES seg11:0x934f-0x9364]
            if ((F.size & 1) != 0) {                     // `test cl, 1` [BYTES seg11:0x9374-0x9377]
                F.size = sar16(shl16(add16(F.size, 1), S), 1);  // [BYTES seg11:0x9379-0x9382]
                F.gap = sar16(shl16(4, S), 1);                  // [BYTES seg11:0x938c-0x9397 seg11:0x93ca]
            } else {
                F.gap = sar16(shl16(6, S), 1);                  // [BYTES seg11:0x9399-0x93a8]
            }
        } else {
            // 5. A live fruit: its `size` and `gap`, `extent` kept [BYTES seg11:0x93ae-0x93ca]
            const ObjectRecord& L = p.rec(kPoolMissiles, e);
            ObjectRecord& F = p.rec(kPoolMissiles, m);
            F.size = L.size;
            F.gap = L.gap;
        }
    }
}

void wildcard(MissileCtx& c, int pool, int rec) {
    wildcard(c.w, c.g, c.st.words, c.rng, linked_wildcard_host(), pool, rec);
}

void render_fruit_image(Blitter& out, ImageSlotTable& images, DcId graphic, int h) {
    DcId& slot = images[ix(kFruitSlot)];
    if (slot != kDcNone) {  // SelectObject back, DeleteDC, DeleteObject [BYTES seg11:0x9146-0x91a6]
        out.delete_image(slot);
        slot = kDcNone;
    }
    if (graphic == kDcNone) return;
    const int g = raw_gap(h);
    const int side = 2 * g + h;  // (gap + gap) + size [BYTES seg11:0x926d-0x9273]
    slot = out.create_image(side, side);  // CreateCompatibleDC + CreateCompatibleBitmap [BYTES seg11:0x91db seg11:0x927b]
    if (slot != kDcNone) {
        out.pat_blt(slot, 0, 0, side, side, kBlackness);                 // [BYTES seg11:0x92af-0x92d9]
        out.bit_blt(slot, g, g, h, h, graphic, 0, 0, kSrcCopy);         // [BYTES seg11:0x92de-0x9309]
    }
    out.delete_image(graphic);  // DeleteDC, DeleteObject [BYTES seg11:0x932d-0x9338]
}

}  // namespace is::world
