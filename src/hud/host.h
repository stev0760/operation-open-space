// What the HUD reads from and calls in the rest of the game: the shell's globals, the wave's
// flags, the player's ship and design records, the target and message texts, and the routines
// of other modules that seg11:0xafe, the panel routines and the IS_INFO paint call. Every
// member names the DGROUP word or routine of INSPACE.EXE it stands for (docs/re/world.md,
// structs.md, globals.md). The defaults are what a wave without that module would show (tests
// override what they need); src/hud/hooks.cpp implements it for the program. Portable.
#pragma once

#include <cstdint>
#include <string>

#include "hud/canvas.h"
#include "shell/geometry.h"

namespace is::hud {

// The fields of one ship record (DGROUP:0x7090 + 0x15a·i) and of its design entry (stride 0xad
// from DGROUP:0x96f0, indexed by the ship's word +0x4e) that the panels read.
struct ShipView {
    int health[4] = {100, 100, 100, 100};  // words +0x26, +0x28, +0x2a, +0x2c [BYTES seg11:0x632]
    int lamp_row[4] = {0, 1, 2, 3};        // design words DGROUP:0x96f4 + 0xad·class + 2k
    bool b_e7 = false;                     // byte +0xe7 (DGROUP:0x7177)
    bool b_e9 = false;                     // byte +0xe9 (DGROUP:0x7179)
    int fuel = 0;                          // word +0x1e (DGROUP:0x70ae)
    int fuel_max = 600;                    // design word DGROUP:0x96f0 + 0xad·class
};

// The weapon the player's ship has selected (seg11:0x238e id 0xdc): w = signed byte +0xb8 +
// slot (DGROUP:0x7148 + j), slot = word +0x18, and the weapon table DGROUP:0x2f54 + 0x50·w.
struct WeaponView {
    int name_id = 0;          // word DGROUP:0x2f54 + 0x50·w: an ENG_LANG string id
    bool counted = false;     // word DGROUP:0x2f5e + 0x50·w != 0
    bool no_count = false;    // word DGROUP:0x2f82 + 0x50·w != 0
    int count = 0;            // signed byte DGROUP:0x713e + j (+0xae + slot)
};

class Host {
public:
    virtual ~Host() = default;

    // ---- the shell
    virtual int state() const { return 1; }                  // DGROUP:0x90f4
    virtual shell::PlayArea area() const { return {}; }      // DGROUP:0x90ea, 0x10, 0x90ec, 0x8e5a
    virtual bool border() const { return true; }             // DGROUP:0xa78, INI Border
    virtual std::uint32_t now() const { return 0; }          // DGROUP:0x1e50
    virtual std::string lang(int) { return {}; }             // LoadString(DGROUP:0x9212, id)
    // DGROUP:0x90f0: 1 at 8 bits per pixel and more. The port's surface is 8-bit (ADR 0007), so
    // the program answers 1.
    virtual bool colour_8bit() const { return true; }
    // DGROUP:0x90ee == 1: a 1-bit display (the target title is then white).
    virtual bool monochrome() const { return false; }
    virtual void log(const std::string&) {}
    // srand(GetCurrentTime()) of seg11:0xafe [BYTES seg11:0xb22 seg11:0xb28] (rng.md "Seeds").
    virtual void seed_rng() {}
    // seg11:0xb0c-0xb1f: the radar's centre (Radar::set_centre).
    virtual void radar_centre() {}

    // ---- the wave's flags
    virtual bool race() const { return false; }      // DGROUP:0x94e
    virtual bool demo_gate() const { return false; } // DGROUP:0x9130
    virtual bool alt_view() const { return false; }  // DGROUP:0x951a
    virtual int player() const { return 0; }         // DGROUP:0x9646
    virtual int ship_pool() const { return 14; }     // DGROUP:0x9324
    virtual int watch_ship() const { return -1; }    // DGROUP:0x952e
    virtual int view_mode() const { return 0; }      // DGROUP:0x9e49
    // DGROUP:0x93fe, toggled every 300 ms by seg11:0x1bcc (world.md "Status tick"; C096)
    // [BYTES seg11:0x1c36 seg11:0x1c3e].
    virtual bool blink() const { return false; }

    // ---- the panels' data
    virtual std::uint32_t score() const { return 0; }         // dword DGROUP:0x1e58
    virtual bool score_alt() const { return false; }          // DGROUP:0x951c
    virtual bool score_alt_hi() const { return false; }       // DGROUP:0x104c
    virtual std::uint32_t wave_ticks() const { return 0; }    // dword DGROUP:0x1e68
    // The clock's write of DGROUP:0x934e, the wave's seconds [BYTES seg11:0x1f1e].
    virtual void set_wave_seconds(int) {}
    virtual ShipView ship(int) const { return {}; }
    virtual WeaponView weapon() const { return {}; }
    // seg3:0x6ec(ship, 7, 0, id): the status message the lamp and fuel routines raise [OPEN].
    virtual void message(int /*ship*/, int /*id*/) {}

    // DGROUP:0xa76, the left IS_INFO display's mode (1 in the file; 2 target, 3 message, 4
    // frame rate). The program keeps it in the World, which the flight role shares.
    virtual int info_mode() const { return info_mode_; }
    virtual void set_info_mode(int m) { info_mode_ = m; }

    // ---- the frame-rate display (seg11:0x238e mode 4)
    virtual int frame_rate_set() const { return 36; }     // DGROUP:0x59c, FrameRateSet
    virtual int frame_rate_actual() const { return 36; }  // DGROUP:0x59a

    // ---- the target and message displays
    // seg13:0x2688(a, b): the attitude of ship a towards ship b.
    virtual int attitude(int /*a*/, int /*b*/) const { return 0; }
    // seg3:0x0(pool, rec): the object can be a target.
    virtual bool target_ok(int /*pool*/, int /*rec*/) const { return true; }
    // seg13:0x371c(pool, rec).
    virtual int target_kind(int /*pool*/, int /*rec*/) const { return 0; }
    // DGROUP:0x9498, the bad-index counter of structs.md: seg11:0x238e bumps it.
    virtual void bump_bad_index() {}
    // The texts the target and message displays draw (filled by seg3:0xf4 and the message
    // routine, [OPEN] world.md "Target text").
    virtual std::string text_9628() const { return {}; }  // DGROUP:0x9628, the target's name
    virtual std::string text_8718() const { return {}; }  // DGROUP:0x8718
    virtual std::string text_960a() const { return {}; }  // DGROUP:0x960a
    virtual std::string text_8740() const { return {}; }  // DGROUP:0x8740, the sender
    virtual std::string text_8768() const { return {}; }  // DGROUP:0x8768
    virtual std::string text_87b8() const { return {}; }  // DGROUP:0x87b8
    // seg13:0x58dc(hdc, 1): the target's icon in the left panel [OPEN]; the canvas view is the
    // panel's.
    virtual void draw_target_icon(Canvas&) {}
    // seg11:0x41bc(hdc, 1, 1): the race display that replaces the target display [OPEN].
    virtual void draw_race(Canvas&) {}

    // ---- the direction marks (seg13:0x24aa) and the target codes (seg3:0x45a)
    // The player's target: ship words +0x3e (pool) and +0x40 (record), DGROUP:0x70ce/0x70d0.
    virtual int player_target_pool() const { return -1; }
    virtual int player_target_rec() const { return 0; }
    // seg13:0x2206: a new target for the player, pool·256 + record [OPEN].
    virtual int next_target() { return -1; }
    // seg3:0xf4(ship, pool, rec): makes (pool, rec) the ship's target and fills the target
    // texts [OPEN]; for the player it also sets the target display's object (Info::mark(0)).
    virtual void set_target(int /*ship*/, int /*pool*/, int /*rec*/) {}
    // An object's screen point: (+0x00, +0x02) when word +0x1a != 0, else seg8:0x29ea of its
    // world position dword +0x04 [BYTES seg13:0x2577-0x25aa].
    virtual Point screen_point(int /*pool*/, int /*rec*/) const { return {}; }
    // The player's (+0x00, +0x02) [BYTES seg13:0x25b0-0x25cb].
    virtual Point player_point() const { return {}; }
    // seg8:0x19b0(dx, dy): the direction 0..15 of a screen offset [OPEN].
    virtual int direction16(int /*dx*/, int /*dy*/) const { return 0; }
    // DGROUP:0x9406 + 2·pool, the pool's in-use count (structs.md "Pools").
    virtual int in_use(int /*pool*/) const { return 0; }
    // seg8:0x285a(world position of a, world position of b): the distance of two objects in
    // 1/8-unit cells, one Newton step of the root (world.md "World coordinates", "Distance";
    // C030) [BYTES seg8:0x285a seg8:0x29c8].
    virtual int distance(int /*pool_a*/, int /*rec_a*/, int /*pool_b*/, int /*rec_b*/) const {
        return 0x7fff;
    }
    // seg3:0x65e case 3: word DGROUP:0x3d6c + 0x34·rec = 2 and word DGROUP:0x3d5a + 0x34·rec > 0
    // (the pool-8 record table) [BYTES seg3:0x6a8-0x6b8].
    virtual bool gate_post_live(int /*rec*/) const { return false; }
    // DGROUP:0x9422, the record bound seg3:0x45a tests for pools above 14 [BYTES seg3:0x574].
    virtual int record_bound_9422() const { return 0; }
    // DGROUP:0x95fc, the window of direction mark 2; non-zero runs seg13:0x24aa(2).
    virtual bool mark2_window() const { return false; }
    // seg11:0x3ed8(2) draws mark 2 into window DGROUP:0x95fc, which the HUD does not own.
    virtual void draw_mark2() {}

    // ---- the state-1 screen of seg11:0x15b9 (implementer D)
    virtual void demo_mark() {}     // word +0x1c = 1 of the record at far pointer DGROUP:0x6da4
    virtual void pin_player() {}    // seg7:0x11cc
    virtual void draw_player() {}   // seg7:0x1208
    virtual void set_58c() {}       // DGROUP:0x58c = 1
    virtual void draw_1f38() {}     // seg7:0x1f38
    virtual void radar() {}         // seg11:0x33cc

protected:
    int info_mode_ = 1;  // [BYTES DGROUP:0xa76]
};

}  // namespace is::hud
