// The wave's update (docs/re/world.md "Update loop"; docs/re/timing.md "Wave passes", "Game
// time"): the work of a wave pass and of a wave frame in the original's address order, the ship
// entry seg7:0x1d0c, the rock mover seg7:0x3bae and the calls of the movers this stage does not
// port, each a named stub that cites its address (ADR 0013 decision 8).
//
// ADR 0001: the port runs one pass before every frame and no pass machinery; the pass's
// frame-gated work runs on that pass, the adaptive thresholds (seg7:0x9ed-0xaf7) are not
// ported, and game time counts frames (DGROUP:0x1e68, DGROUP:0x5a8). [DESIGN ADR 0001]
//
// Portable, no Win32. Calls into other parts of the port go through UpdateHost, whose defaults
// do nothing.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "world/capture.h"
#include "world/collide.h"
#include "world/draw.h"
#include "world/flight.h"
#include "world/input.h"
#include "world/shots.h"
#include "world/sprite.h"
#include "world/view.h"
#include "world/visibility.h"
#include "world/world.h"

namespace is::world {

// The movers of seg7:0xcd3-0xd2e that other stages port, by address.
enum class Mover : std::uint8_t {
    k12_532c,  // pool 9
    k12_633e,  // pool 8 (gates and hazards)
    k8_8256,   // pool 2
    k7_309c,
    k7_32d0,
    k7_40c4,   // pool DGROUP:0x9328 = 5
    k12_0,     // pool 6
    k6_73f6,
    k6_7c14,
    k7_5e04,   // while DGROUP:0x9456 > 0
    k12_3148,  // pool 10
    k12_6b1e,  // pool 11
    k12_ae4,   // pool 4, while DGROUP:0x9600 != 0
    k12_3e98,  // seg12:0x3e98(0)
};
const char* mover_address(Mover m);

// The DGROUP words of the pass and the frame beyond WaveGlobals.
struct FrameGlobals {
    std::int16_t dg_17b6 = 0;           // DGROUP:0x17b6, the 8-frame counter
    std::uint32_t info_due = 0;         // DGROUP:0x17b8, the information displays' deadline
    std::uint32_t joy_due = 0;          // DGROUP:0x2388, the joystick read's deadline
    std::int16_t dg_9142 = 0;           // DGROUP:0x9142, the 0 ... 54 frame counter
    // DGROUP:0x9430, 0x9434 and 0x9456 are the World's (World::w9430, w9434, w9456): the pass
    // reads them through UpdateHost::frame_world.
    // DGROUP:0x594, 0x598, 0x5cc and 0x9136, the shot loop's words, are shots.h ShotState's
    // (UpdateHost::shots()).
    std::int16_t entry_pending = 0;     // DGROUP:0x9140: a ship waits off-world (state 100)
    std::int16_t dg_9146 = 0;           // DGROUP:0x9146
    std::int16_t size_reread = 0;       // DGROUP:0x103c
    std::uint32_t speed_shown = 0;      // DGROUP:0x870c, the end of the speed display
    std::int16_t dg_9608 = 0;           // DGROUP:0x9608
    std::int16_t info_mode = 2;         // DGROUP:0xa76: 2 target, 3 message, 4 frame rate
    std::int16_t dg_95fc = 0;           // DGROUP:0x95fc
    std::uint32_t dg_6da8 = 0;          // DGROUP:0x6da8
    std::int16_t dg_5d2 = 0;            // DGROUP:0x5d2, toggled each frame
    std::int16_t dg_9600 = 0;           // DGROUP:0x9600
    std::uint32_t music_due = 0;        // DGROUP:0x2384
    std::int16_t dg_950 = 0;            // DGROUP:0x950
    std::int16_t race = 0;              // DGROUP:0x94e, the race flag
    std::int16_t dg_58a = 0;            // DGROUP:0x58a
    std::int16_t dg_588 = 0;            // DGROUP:0x588
    std::int16_t dg_9148 = 0;           // DGROUP:0x9148
    std::int16_t dg_1048 = 0;           // DGROUP:0x1048
    std::uint32_t dg_1e6c = 0;          // DGROUP:0x1e6c
    std::int16_t dg_92f0 = 0;           // DGROUP:0x92f0
    std::int16_t dg_914e = 0;           // DGROUP:0x914e
    std::uint32_t dg_5a4 = 0;           // DGROUP:0x5a4
    std::uint32_t dg_5b8 = 0;           // DGROUP:0x5b8
    std::int16_t dg_943a = 0, dg_943c = 0;  // DGROUP:0x943a, 0x943c: the colliding pool, record
    std::int16_t dg_9454 = 0;           // DGROUP:0x9454
};

struct UpdateCtx;

class UpdateHost : public FlightHost {
public:
    // ---- the pass
    virtual void fuel_mass() {}                        // seg11:0x1bcc
    virtual void turn_followup() {}                    // seg11:0x894 [OPEN]
    virtual void radar_refresh() {}                    // seg11:0x3396
    virtual void select_clip() {}                      // SelectClipRgn(DGROUP:0x13dc, DGROUP:0x12)
    // seg11:0x4e0a(ship), the gun: the default runs shots.h fire on the frame World with
    // shots() and shot_host(); nothing without a World (M4 s2 fixer).
    virtual void fire(int ship);
    virtual void seg8_9464() {}                        // [OPEN]
    virtual void seg8_9564() {}                        // [OPEN]
    virtual void seg7_2128() {}  // seg7:0x2128, the violence decay (aggression.h; the program's host runs it)
    // seg8:0x3aec(j, i), slot j's sub-step i: the default runs shots.h shot_update on the frame
    // World, 0 without one (M4 s2 fixer).
    virtual std::int16_t beam(int i, int j);
    // The shot slots DGROUP:0x6098 and their words (shots.h); the default is shot_state().
    virtual ShotState& shots() { return shot_state(); }
    // The gun's and the shot update's calls out; the default forwards the reset sound (play),
    // seg13:0x4eec, the player's sprite, the thrust step and the brushes and PatBlts on the
    // window DC (blitter(), WaveGraphics::star = DGROUP:0x3b4c).
    virtual ShotHost& shot_host() { return shot_forward_; }
    // The shot context of the frame (the collisions' context, shots(), shot_host(),
    // effects.h effect_state()) runs f; false without a frame World.
    template <typename F>
    bool with_shots(F f);
    virtual void effect(int k) { (void)k; }            // seg8:0x3fbe
    virtual void burst() {}                            // seg7:0x375a, the particle burst
    // ---- the frame
    // ISPACE.DLL ordinal 41 as seg11:0x72b2 calls it: fills far DGROUP:0x1e4c and returns the
    // read's result, 0xa7 (JOYERR_UNPLUGGED) for no stick [BYTES seg11:0x72c9 seg11:0x72ce].
    virtual std::uint16_t joy_read() { return 0; }
    // PostMessage(main DGROUP:0x16, msg, key, 0) of a joystick button (seg11:0x7588).
    virtual void post_key(std::uint32_t msg, std::uint16_t key) { (void)msg, (void)key; }
    virtual void seg18_frame() {}                      // seg16:0x64fa in the seg18 mode
    virtual void size_reread() {}                      // seg10:0x7f88
    virtual void stars(int arg) { (void)arg; }         // seg8:0x158a(0)
    virtual void visibility() {}                       // seg8:0x4e14 [OPEN]
    virtual void speed_display_end_3_6ec() {}          // seg3:0x6ec(player, 14, +0x58, 2)
    virtual void speed_display_end_7_119c() {}         // seg7:0x119c(2, 0)
    virtual void lines() {}                            // seg12:0x3778
    virtual void dissolve(int i) { (void)i; }          // seg7:0x3a42(i)
    virtual void direction_mark(int d) { (void)d; }    // seg13:0x24aa(d)
    virtual void seg16_a2() {}
    virtual bool seg16_33a2() { return false; }
    virtual void seg16_3306() {}
    virtual void seg11_a9ea(int arg) { (void)arg; }
    virtual void sound_frame() {}                      // seg10:0x7b48 (M7's audio::frame)
    virtual void seg12_4d8e() {}
    virtual void seg18_d94() {}
    virtual void mover(Mover m) { (void)m; }           // the movers this stage does not port
    virtual void music_due() {}                        // seg13:0xba04
    virtual void seg13_b93a() {}
    virtual void seg7_158e() {}
    virtual void seg13_447a() {}
    // ---- the ship entry seg7:0x1d0c
    virtual void ship_entered(int ship) { (void)ship; }  // seg12:0x2556 and seg7:0x13b2(2, 14, i)
    virtual void seg13_563c() {}
    // seg7:0x1e4e: the player's design word DGROUP:0x9714 + 0xad·n (`captures`) = 1, so the player
    // always captures (core.md "Who captures"); the default writes the frame World's
    // design_ratings[n].captures (M4 s2 CAPTURE) [BYTES seg7:0x1e4e].
    virtual void set_design_word_22(int design, std::int16_t v);
    // ---- the rock mover seg7:0x3bae and the gate posts seg12:0x1776
    // SelectObject(DGROUP:0x13dc, DGROUP:0x3b74[b]) and back: the heated rock's or post's brush.
    // The defaults select wave_graphics().heat[b] on blitter()'s window DC (M3 s4 DRAW).
    virtual std::uint16_t select_rock_brush(int b);
    virtual void restore_brush(std::uint16_t old);
    // seg8:0x50aa(pool * 256 + rec): the collision test; 0xFFFF for none (core.md "Who tests
    // whom"). The default runs collide.h find_pair on the frame's World (frame_world), 0xffff
    // without one (M4 s1 COLLIDE).
    virtual std::uint16_t collide(std::int16_t pool_rec);
    // seg7:0x4014-0x4060: amount = seg8:0x546c(rock mass, extent + 2, &vy, &vx, hit), then
    // seg8:0x5df2(DGROUP:0x934a, rock, hit pool, hit rec, amount), its result ignored
    // (s1-hit.md 4) [BYTES seg7:0x4049 seg7:0x404e seg7:0x4060 seg7:0x4065].
    virtual void rock_hit(int rock, std::uint16_t hit);

    // ---- the drawing the frame does itself (M3 s4 DRAW)
    // The World whose pools the frame moves: the gates' records, the stars, the link slots and
    // the pool-8 table live there, not in FlightCtx. The default is the World set_frame_world
    // registered (the program's, src/world/hooks.cpp) when its pools are `pools`, else null;
    // with null the frame calls stars(), lines() and mover(k12_633e) instead [DESIGN].
    virtual World* frame_world(Pools& pools);
    // The window's Blitter for the draws beside seg8:0xb76 (the beam, the stars); the default
    // is wave_blitter(). With null the frame calls stars() and draws no beam.
    virtual Blitter* blitter() { return wave_blitter(); }
    // The list of the frame's visibility pass (DGROUP:0x507c / 0x50e0, count DGROUP:0x9146);
    // the default is last_visibility().
    virtual const VisibilityGlobals* visible_list() { return last_visibility(); }
    // The routines seg12:0x633e calls for the hazards (M5): seg12:0x390e(i) (kind 1),
    // seg12:0x5636(i) (kind 2), seg12:0x7006 (every other frame). The M5 port of seg12:0x5636
    // must clear World::w949e (DGROUP:0x949e) as it does at seg12:0x5ef8, which gates the
    // hazard marks' erase [BYTES seg12:0x5ef8 seg12:0x6408] (M3 s4 fixer, review L12).
    virtual void hazard(int kind, int i) { (void)kind, (void)i; }
    virtual void seg12_7006() {}
    // seg12:0x1d79-0x1dc3: seg8:0x546c(post mass, extent + 2, &vy, &vx, hit), then
    // seg8:0x5df2(8, r, hit pool, hit rec, amount), its result ignored (s1-hit.md 4)
    // [BYTES seg12:0x1da9 seg12:0x1dae seg12:0x1dbe seg12:0x1dc3].
    virtual void post_hit(int rec, std::uint16_t hit);
    // seg12:0x2288(pool, rec): a ship passes the gate, the gate exit [OPEN: not read].
    virtual void gate_passed(int pool, int rec) { (void)pool, (void)rec; }
    // seg13:0x2f30 after seg12:0x1f7a's AI words for ship k (M5).
    virtual void gate_ai(int ship) { (void)ship; }
    // seg12:0x3778's erase and its seg7:0x2080 call (draw.h LinkHooks); null draws nothing.
    virtual LinkHooks* link_hooks() { return nullptr; }
    // seg8:0x1320, the Bright starfield [OPEN].
    virtual void bright_stars(int arg) { (void)arg; }

    // ---- the collisions (collide.h, damage.h) (M4 s1 COLLIDE, M4 s2 CAPTURE)
    // seg13:0xb5a-0xcf9 for an on-screen ship i (work/analysis/m4/s2-capture.md 1a, verified
    // claims 5-10): DGROUP:0x943c = i, DGROUP:0x943a = DGROUP:0x9324, the pair finder; on a hit
    // the response always, then the touch seg13:0x16ac for a hit object with `capturable` (+0x53)
    // != 0 and ship byte +0xea = 0, or for a kind-6 missile (dead code in a normal wave), else the
    // dispatcher; then, hit or not, the weapon tail seg13:0xc95-0xcf6 (weapons 24 ... 26).
    void ship_collide(int ship) override;
    // seg18:0x551a(DGROUP:0x9324, i) (weapons 0x18 and 0x1a, every frame) and seg18:0x59ec
    // (weapons 0x19 and 0x1a, frame# & 7 = 0 with Enter or Z down, any ship i) [BYTES seg13:0xca6
    // seg13:0xcb7 seg13:0xcbf-0xcf1]: the "noble forces" bodies are [OPEN].
    virtual void seg18_551a(int pool, int ship) { (void)pool, (void)ship; }
    virtual void seg18_59ec(int pool, int ship) { (void)pool, (void)ship; }

    // ---- the touch routine's calls out (capture.h CaptureHost) (M4 s2 CAPTURE)
    // The default forwards erase (blitter()), the first dissolve step (dissolve()), the sounds
    // (sound(), play()), the messages (collide_host().message), lang(), weapon_word() and the
    // team (design_kind); the M5/M6 routines stay CaptureHost's stubs.
    virtual CaptureHost& capture_host() { return capture_forward_; }
    // seg10:0x744c(id) (src/audio play) [DESIGN: the Win32 host plays it].
    virtual void play(int id) { (void)id; }
    // An ENG_LANG string (LoadString), "" by default.
    virtual std::string lang(int id) { return (void)id, std::string{}; }
    // W(w)+off of the weapon table DGROUP:0x2f54: +0x24 through collide_host().weapon_kind(w),
    // any other word from collide.h weapon_word (the program's mutable copy, M4 s2 fixer).
    virtual std::int16_t weapon_word(int w, int off) {
        return off == 0x24 ? collide_host().weapon_kind(w) : ::is::world::weapon_word(w, off);
    }

    // The collision routines' calls out; the default forwards sound(), seg12_274a() (the
    // destroy), the flame-off (flight.h ship_touched) and the erase (blitter()) and leaves the
    // rest to CollideHost's defaults.
    virtual CollideHost& collide_host() { return forward_; }
    // The collision DGROUP words; the default is this host's own.
    virtual CollideGlobals& collide_globals() { return collide_globals_; }
    // ---- M4 s3 FUEL: the cloak, the shield and the small movers (pools 1, 4 and 11)
    // seg11:0x4c72(i), seg11:0x4b00(i) and seg3:0x332e(ids) from the ship mover: input.h
    // cloak_toggle, shield_toggle and strategy_notice on the frame World with systems_host();
    // 0 without a frame World.
    std::int16_t seg11_4c72(int ship) override;
    std::int16_t seg11_4b00(int ship) override;
    void seg3_332e(std::uint32_t ids) override;
    // The image table DGROUP:0x39e8; the default is small_mover_links().images (null: none).
    virtual const ImageSlotTable* images();
    // seg12:0xe5c(i) and seg12:0x129e(pool, rec), the pickup dissolves (tank.h, pods.h) on the
    // frame World, drawn with blitter() and images().
    virtual void tank_dissolve(int rec);
    virtual void pod_dissolve(int pool, int rec);
    // DGROUP:0x4e94 + 2r != 0: pool-1 record r has its own bitmap; and seg7:0x32d0's draw of it:
    // SelectObject(DGROUP:0x9288, DGROUP:0x4e94[r]), seg8:0xb76(pool_rec, 3, DGROUP:0x9288, 0),
    // the old bitmap back [BYTES seg7:0x333d-0x3388]. The defaults use small_mover_links().
    virtual bool fragment_has_bitmap(int r);
    virtual void draw_fragment(std::int16_t pool_rec, int r);

    // The frame's context while the movers run (set by wave_frame, move_rocks, move_gate).
    UpdateCtx* frame_ctx = nullptr;

    UpdateHost() { forward_.u = this; capture_forward_.u = this; shot_forward_.u = this; }
    UpdateHost(const UpdateHost&) = delete;
    UpdateHost& operator=(const UpdateHost&) = delete;

private:
    class Forward final : public CollideHost {
    public:
        UpdateHost* u = nullptr;
        void sound(int id, int pool, int rec, int mode) override { u->sound(id, pool, rec, mode); }
        void destroy(int pool, int rec) override { u->seg12_274a(pool, rec); }
        void flame_off(int ship) override;
        void erase(int x, int y, int w, int h) override;
        void puff(int pool, int rec, int arg) override { u->puff(pool, rec, arg); }
    };
    class CaptureForward final : public CaptureHost {
    public:
        UpdateHost* u = nullptr;
        void erase(int x, int y, int w, int h) override;
        void dissolve(int rec) override { u->dissolve(rec); }
        void sound(int id, int pool, int rec, int mode) override { u->sound(id, pool, rec, mode); }
        void play(int id) override { u->play(id); }
        void message(int ship, int pool, int rec, int code) override {
            u->collide_host().message(ship, pool, rec, code);
        }
        std::string lang(int id) override { return u->lang(id); }
        std::int16_t weapon_word(int w, int off) override { return u->weapon_word(w, off); }
        std::int16_t team(int design) override { return u->design_kind(design); }
        void seg13_b6f2(int ship, int code) override { u->collide_host().seg13_b6f2(ship, code); }
        void seg13_2714(int a, int b, int c, int d, int e, int f) override {
            u->collide_host().seg13_2714(a, b, c, d, e, f);
        }
        void seg4_5e8e(int a, int b, int c, int d) override { u->collide_host().seg4_5e8e(a, b, c, d); }
        // The touch's first dissolve step of a tank (seg13:0x1cd8) and of a pod or pool-6 record
        // (seg13:0x1f59) (M4 s3 FUEL).
        void seg12_e5c(int rec) override { u->tank_dissolve(rec); }
        void seg12_129e(int pool, int rec) override { u->pod_dissolve(pool, rec); }
    };
    class ShotForward final : public ShotHost {
    public:
        UpdateHost* u = nullptr;
        void sound_744c(int id) override { u->play(id); }
        void seg13_4eec(int ship, int arg) override;
        void player_sprite() override { u->player_sprite(); }
        void thrust_step(int mode, int ship) override;
        void select_brush(int n) override;
        void patblt(std::int16_t x, std::int16_t y, std::int16_t w, std::int16_t h, std::uint32_t rop) override;
    };
    Forward forward_;
    CaptureForward capture_forward_;
    ShotForward shot_forward_;
    CollideGlobals collide_globals_;
};

// The program's World for UpdateHost::frame_world (null: none) (M3 s4 DRAW).
void set_frame_world(World* w);

// The drawing side's parts of the small movers (src/hud/wave_win32.cpp fills them; an empty slot
// draws nothing) (M4 s3 FUEL).
struct SmallMoverLinks {
    std::function<const ImageSlotTable*()> images;          // DGROUP:0x39e8
    std::function<bool(int r)> fragment_has_bitmap;         // DGROUP:0x4e94 + 2r != 0
    std::function<void(std::int16_t pool_rec, int r)> draw_fragment;  // seg7:0x333d-0x3388
};
SmallMoverLinks& small_mover_links();

// What the pass and the frame touch.
struct UpdateCtx {
    FlightCtx flight;
    FrameGlobals& f;
    ViewState& view;
    UpdateHost& host;
    JoyState& joy;
    JoyButtons& joy_buttons;
};

// The collision context of the frame on World w (below).
CollideCtx frame_collide_ctx(UpdateCtx& c, World& w);

// The frame's context on the host while a mover runs; a nested scope keeps the outer one (the
// movers of M4 s3 FUEL, called directly by tests as by wave_frame).
struct MoverScope {
    UpdateHost& h;
    UpdateCtx* prev;
    MoverScope(UpdateHost& host, UpdateCtx& c) : h(host), prev(host.frame_ctx) {
        if (!prev) h.frame_ctx = &c;
    }
    ~MoverScope() { h.frame_ctx = prev; }
    MoverScope(const MoverScope&) = delete;
    MoverScope& operator=(const MoverScope&) = delete;
};

template <typename F>
bool UpdateHost::with_shots(F f) {
    if (!frame_ctx) return false;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return false;
    CollideCtx x = frame_collide_ctx(*frame_ctx, *w);
    ShotCtx sc{x, shots(), shot_host(), effect_state()};
    f(sc);
    return true;
}

// The wave pass (seg7:0x7fe-0x993) before a due frame. `max_rate` is FrameRateSet above 1000,
// where the original's threshold is 0 and the turn follow-up waits for frames % 3 = 0.
void wave_pass(UpdateCtx& c, bool max_rate);
// The wave frame (seg7:0x9ce-0x118d).
void wave_frame(UpdateCtx& c);
// seg7:0x1d0c: the first off-world ship whose time has come enters at a gate.
void ship_entry(UpdateCtx& c);
// seg7:0x3bae: the rock mover.
void move_rocks(UpdateCtx& c);
// The collision context of the frame on World w: the host's collision words and calls, the
// frame's view, visibility list, rand() and DGROUP:0x943a / 0x943c / 0x9454 (M4 s2 CAPTURE).
CollideCtx frame_collide_ctx(UpdateCtx& c, World& w);

// ---- the pool-8 mover seg12:0x633e (work/analysis/m3/s4-draw.md 1) (M3 s4 DRAW)

// seg12:0x633e [BYTES seg12:0x633e-0x649f]: DGROUP:0x94a0, seg12:0x7006 every other frame, the
// oscillator DGROUP:0x0d08, the kind dispatch of the in-use pool-8 records with `state` 0 (kind
// 1 and 2 go to the host, kind 3 runs seg12:0x1776 for the pair and skips the second post), the
// hazard marks' erase and the two deadlines.
void move_pool8(UpdateCtx& c, World& w);
// seg12:0x1776(p) [BYTES seg12:0x1776-0x1f75]: gate p's posts 2p, 2p + 1: the beam brush
// index, the moving gate, the posts through seg8:0xb76 from slot 1 (heat <= 2, SRCCOPY) or
// slot 2 under DGROUP:0x3b74 (MERGECOPY), the beam (r1 only: the erase of the old position,
// then PATCOPY with DGROUP:0x3b88[index]), the OUT sign flash, the collisions and the passage.
void move_gate(UpdateCtx& c, World& w, int p);
// seg12:0x1f7a(p) [BYTES seg12:0x1f7a-0x2285]: the first listed object of `state` 0 whose square
// (x, y, x + extent, y + extent) meets gate p's beam rectangle, not pool 7, mass not 0xdac, a
// ship only after its dword +0x88 + 300 (the player) or + 3000, and byte +0x2e != 1 unless a
// ship with word +0x2e = 2: pool << 8 | rec, else 0xffff. A computer ship on its way to the
// gate stops and gets its AI words on the way.
std::uint16_t gate_test(UpdateCtx& c, World& w, int p);

}  // namespace is::world
