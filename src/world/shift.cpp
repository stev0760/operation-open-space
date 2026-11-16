// Perspective shifting (shift.h): seg11:0x6dc4, seg7:0x7d5 and seg11:0xaece, ported from the
// listing [BYTES seg11:0x6d19-0x6dd3 seg7:0x7d5-0x7dc seg11:0xaece-0xb09a].
#include "world/shift.h"

#include "world/flight.h"
#include "world/view.h"
#include "world/world.h"

namespace is::world {

ShiftWords& shift_words() {
    static ShiftWords s;
    return s;
}

void shift_key_up(World& w) {
    if (w.player_game == 0) return;  // DGROUP:0x95bc = 0 [BYTES seg11:0x6d19-0x6d1e]
    if (w.w920c == 3) return;        // [BYTES seg11:0x6dc4-0x6dc9]
    shift_words().latch = 1;         // [BYTES seg11:0x6dcd]
}

namespace {

bool ship_ok(const World& w, int i) {
    return i >= 0 && i < kShipCount && w.pools.records(w.roles.ships) > i;
}

}  // namespace

std::int16_t perspective_shift(World& w, ViewState& v, ShiftHost& h, WaveGlobals* frame) {
    if (w.kind == 6 || w.kind == 7) return 0;  // [BYTES seg11:0xaedc-0xaeec]
    shift_words().latch = 0;                   // [BYTES seg11:0xaef1]
    const std::int16_t p = w.player;           // [bp-4]
    if (!ship_ok(w, p)) {
        static bool once = false;
        guard_todo(once, "seg11:0xaece with DGROUP:0x9646 outside the ship records", "seg11:0xaefe");
        return 0;
    }
    // The ship pool through DGROUP:0x1df8 + 4·DGROUP:0x9324 [BYTES seg11:0xaf01-0xaf10].
    if (w.ship_object(p).state != 0) return 0;
    if (w.handover.demons_gate != 0) return 0;  // [BYTES seg11:0xaf14-0xaf18]
    const auto team = [&w](int i) {
        const int n = w.ships[static_cast<std::size_t>(i)].design_index;
        return n >= 0 && n < kShipCount ? w.design_ratings[static_cast<std::size_t>(n)].team : std::int16_t{0};
    };
    // [BYTES seg11:0xaf1a-0xaf94]
    std::int16_t cur = p;
    for (;;) {
        if (w.last_ship > cur)
            cur = add16(cur, 1);  // [BYTES seg11:0xaf23]
        else
            cur = 0;  // [BYTES seg11:0xaf29]
        w.player = cur;
        bool ok = true;  // [bp-0xa]
        if (!ship_ok(w, cur)) {
            static bool once = false;
            guard_todo(once, "seg11:0xaece's search past the ship records", "seg11:0xaf34");
            ok = false;
        } else if (w.ship_object(cur).state != 0 || w.ship(cur).bec == 0) {
            ok = false;  // [BYTES seg11:0xaf44-0xaf58]
        }
        if (cur == p) {
            ok = true;  // [BYTES seg11:0xaf65]
        } else if (ship_ok(w, cur) && team(cur) != team(p)) {
            ok = false;  // [BYTES seg11:0xaf6c-0xaf8b]
        }
        if (ok) break;
    }
    if (cur == p) return 0;  // [BYTES seg11:0xaf96-0xaf9c]
    const std::int16_t s = cur;  // [bp-6]
    w.player = p;                // [BYTES seg11:0xafa6]
    ShipRecord& S = w.ship(s);
    ShipRecord& P = w.ship(p);
    // [BYTES seg11:0xafa9-0xafda]: +0xd0 (objects destroyed) and +0xd2 (icons captured).
    for (int off : {0xd0, 0xd2}) {
        const std::int16_t t = word_at(S, off);
        set_word_at(S, off, word_at(P, off));
        set_word_at(P, off, t);
    }
    h.message(p, w.roles.ships, s, 0x71);  // seg3:0x6ec(p, DGROUP:0x9324, s, 0x71) [BYTES seg11:0xafde-0xafef]
    w.player = s;                           // [BYTES seg11:0xaff5]
    if (S.design_index >= 0 && S.design_index < kShipCount)  // design word +0x42 = 1 [BYTES seg11:0xaff8-0xaffe]
        w.design_ratings[static_cast<std::size_t>(S.design_index)].captures = 1;
    if (frame) frame->player = s;
    // seg13:0x2206() and seg3:0xf4(DGROUP:0x9646, t >> 8, t & 0xff), both bytes zero-extended
    // [BYTES seg11:0xb004-0xb01f].
    const int t = h.next_target();
    h.set_target(s, (t >> 8) & 0xff, t & 0xff);
    // seg8:0x4d60(&ship s +0xd6, &ship DGROUP:0x9646 +0xd6): the same word twice, no exchange
    // (correction 61; pitfalls.md) [BYTES seg11:0xb022-0xb033].
    w.player_roster = S.roster_index;  // DGROUP:0x931c [BYTES seg11:0xb038-0xb042]
    // [BYTES seg11:0xb045-0xb04c]
    w.w1e84 = 0;
    w.w1e80 = 0;
    w.play_mode = 1;  // [BYTES seg11:0xb050]
    if (frame) frame->play_mode = 1;
    if (w.w951a == 0) {  // [BYTES seg11:0xb056-0xb070]: dword DGROUP:0x6b50 = p's (x, y)
        const ObjectRecord& o = w.ship_object(p);
        v.cam_x = o.x;
        v.cam_y = o.y;
    }
    w.w951a = 1;  // [BYTES seg11:0xb077]
    if (frame) frame->autopilot = 1;
    h.lamps(s, 1);       // seg11:0x3cc2(DGROUP:0x9646, 1) [BYTES seg11:0xb07a-0xb084]
    h.weapon_slot();     // seg11:0xae94 [BYTES seg11:0xb088]
    h.weapon_picture();  // seg11:0x2f8c [BYTES seg11:0xb08b]
    return 1;
}

std::int16_t shift_pass(World& w, ViewState& v, ShiftHost& h, WaveGlobals* frame) {
    if (shift_words().latch == 0) return 0;  // [BYTES seg7:0x7d5-0x7da]
    return perspective_shift(w, v, h, frame);
}

ShiftLinks& shift_links() {
    static ShiftLinks l;
    return l;
}

namespace {

class LinkedShiftHost final : public ShiftHost {
public:
    void message(int recipient, int pool, int rec, int code) override {
        if (shift_links().message) shift_links().message(recipient, pool, rec, code);
    }
    int next_target() override { return shift_links().next_target ? shift_links().next_target() : -1; }
    void set_target(int ship, int pool, int rec) override {
        if (shift_links().set_target) shift_links().set_target(ship, pool, rec);
    }
    void lamps(int ship, int arg) override {
        if (shift_links().lamps) shift_links().lamps(ship, arg);
    }
    void weapon_slot() override {
        if (shift_links().weapon_slot) shift_links().weapon_slot();
    }
    void weapon_picture() override {
        if (shift_links().weapon_picture) shift_links().weapon_picture();
    }
    void black_cell(int x, int y, int w, int h) override {
        if (shift_links().black_cell) shift_links().black_cell(x, y, w, h);
    }
    void pin_player() override {
        if (shift_links().pin_player) shift_links().pin_player();
    }
    void draw_player() override {
        if (shift_links().draw_player) shift_links().draw_player();
    }
    void info_mode(int mode) override {
        if (shift_links().info_mode) shift_links().info_mode(mode);
    }
    void todo(const std::string& what, const char* address) override {
        if (shift_links().todo) shift_links().todo(what, address);
    }
};

}  // namespace

ShiftHost& linked_shift_host() {
    static LinkedShiftHost h;
    return h;
}

}  // namespace is::world
