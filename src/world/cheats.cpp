// The cheat keys (cheats.h): seg11:0x6ee7 and seg11:0x72e0 ported from the listing, each key
// with exactly the stores the bytes make (ruling R1), the 16-bit widths of the originals.
#include "world/cheats.h"

#include <set>
#include <string>

#include "shell/rng.h"
#include "world/callin.h"
#include "world/weapon_load.h"
#include "world/world.h"

namespace is::world {

namespace {

// DGROUP:0x9646 read through a ship record or an object record: an index outside the tables
// reads nothing and the key does nothing [DESIGN ADR 0016 decision 2].
bool player_ok(CheatCtx& c, const char* address) {
    const World& w = c.w;
    if (w.player >= 0 && w.player < kShipCount && w.player < w.pools.records(w.roles.ships)) return true;
    static bool once = false;
    guard_todo(once, "a cheat key with the player outside the ship pool", address);
    return false;
}

// The player's roster record DGROUP:0x6e24 + 0x7c·DGROUP:0x931c (seg11:0x72f1, seg11:0x748d).
RosterRecord* player_roster(CheatCtx& c, const char* address) {
    World& w = c.w;
    const int k = w.player_roster;
    if (k >= 0 && static_cast<std::size_t>(k) < w.roster.size()) return &w.roster[static_cast<std::size_t>(k)];
    static bool once = false;
    guard_todo(once, "a powerup key with the roster index outside the roster", address);
    return nullptr;
}

// seg11:0x72ff: sound 0x20 at (DGROUP:0x9324, P) in mode 1, the tail of '9' and F3
// [BYTES seg11:0x72ff-0x730b].
void sound_at_player(CheatCtx& c, int id) { c.h.sound(id, c.w.roles.ships, c.w.player, 1); }

}  // namespace

bool cheat_gate(CheatCtx& c) {
    // [BYTES seg11:0x6ee7-0x6ef3]: DGROUP:0x920c = 2, else -1.
    if (c.w.w920c != 2 && c.w.w920c != -1) return false;
    // `cmp [0x951c], 1; sbb ax, ax; neg ax` [BYTES seg11:0x6ef7-0x6f00].
    c.st.w951c = c.st.w951c == 0 ? 1 : 0;
    if (c.st.w951c == 0)
        c.god = 0;  // [BYTES seg11:0x6f07]
    else
        c.h.play(kSoundCheatOn);  // [BYTES seg11:0x6f0c-0x6f13]
    c.h.score_redraw();           // dword DGROUP:0x5018 = -1 [BYTES seg11:0x6f14]
    return true;
}

void cheat_key(CheatCtx& c, std::uint16_t vk) {
    World& w = c.w;
    // The switch seg11:0x749e: `cmp ax, 0x78; je F9; ja out`, then the subtraction chain. A VK
    // below '1' wraps in AL and reaches no case [BYTES seg11:0x749e-0x74f7].
    if (!is_cheat_vk(vk)) return;
    const int p = w.player;
    switch (vk) {
    case '1':  // seg11:0x73b2 [BYTES seg11:0x73b2-0x73cd]
        if (w.kind != 6) return;  // `cmp [0x9320], 6; jne`: a race only
        // seg4:0x2da2(), then AX = P's laps - DGROUP:0x9ea0 - 1, dead (the caller drops it)
        // [BYTES seg11:0x73b9-0x73cc seg11:0x6d69 seg11:0x6e0d].
        c.h.race_finish();
        return;
    case '2':  // seg11:0x73d0 [BYTES seg11:0x73d0-0x73dd]
        if (w.kind != 7) return;
        w.gate[0].w12 = -1;  // word DGROUP:0x668a = 0xffff
        return;
    case '3':  // seg11:0x73e0: seg13:0xadbe(P, 4, 0x6a) [BYTES seg11:0x73e0-0x73eb seg11:0x732c-0x7335]
        if (w.last_ship >= 8) return;  // signed `jge`
        c.h.call_in(p, kCallInHelp, kMsgKnight);
        return;
    case '4':  // seg11:0x73ee: seg13:0xadbe(P, 3, 0x69) [BYTES seg11:0x73ee-0x73f9]
        if (w.last_ship >= 8) return;
        c.h.call_in(p, kCallInPirates, kMsgPirate);
        return;
    case kVkF5:  // seg11:0x731f: seg13:0xadbe(P, 0x16, 0x6d) [BYTES seg11:0x731f-0x7335]
        if (w.last_ship >= 8) return;
        c.h.call_in(p, kCallInPatrol, kMsgPatrolman);
        return;
    case '5': {  // seg11:0x73fc [BYTES seg11:0x73fc-0x7430]
        if (w.handover.demons_gate == 0) return;  // DGROUP:0x92e8
        if (!player_ok(c, "seg11:0x740c")) return;
        w.w1318 = 1;                                // DGROUP:0x1318
        w.ship_object(p).b54 = 0;                   // object +0x54 through DGROUP:0x1e30, stride 0x70
        w.w913e = 1;                                // DGROUP:0x913e
        c.h.message(p, 7, 0, 0xb5);                 // seg3:0x6ec(P, 7, 0, 0xb5): 1944, "Now Take The Enlightened Path ..."
        return;
    }
    case '6': {  // seg11:0x7433 [BYTES seg11:0x7433-0x7461]
        if (!player_ok(c, "seg11:0x7433")) return;
        ShipRecord& s = w.ship(p);
        // ax = design word DGROUP:0x96f0 + 0xad·(+0x4e); cx = fuel + 250 (16-bit); signed `jle`.
        std::int16_t tank = 0;
        if (s.design_index >= 0 && s.design_index < kShipCount) {
            tank = w.design_ratings[static_cast<std::size_t>(s.design_index)].tank;
        } else {
            static bool once = false;
            guard_todo(once, "the refuel key with the design index outside the design table", "seg11:0x7439");
        }
        const std::int16_t cap = add16(s.fuel, 250);
        s.fuel = tank <= cap ? tank : cap;
        sound_at_player(c, kSoundRefuel);
        return;
    }
    case '7':  // seg11:0x7464 [BYTES seg11:0x7464-0x7470]
        c.st.w104e = c.st.w104e == 0 ? 1 : 0;  // `cmp 1; sbb; neg`
        return;
    case '8': {  // seg11:0x7473 [BYTES seg11:0x7473-0x748a]
        if (!player_ok(c, "seg11:0x7473")) return;
        ShipRecord& s = w.ship(p);
        s.set_resources(add16(s.resources(), 100));  // word +0xce += 0x64: one icon
        sound_at_player(c, kSoundResources);
        return;
    }
    case '9':  // seg11:0x748d: THRUSTERS (+0x33) = 6 [BYTES seg11:0x748d-0x749b seg11:0x72ff-0x7313]
        if (RosterRecord* r = player_roster(c, "seg11:0x748d")) r->b33[0] = 6;
        sound_at_player(c, kSoundPowerup);
        return;
    case kVkF3:  // seg11:0x72f1: GUNS (+0x32) = 6 [BYTES seg11:0x72f1-0x7313]
        if (RosterRecord* r = player_roster(c, "seg11:0x72f1")) r->b32 = 6;
        sound_at_player(c, kSoundPowerup);
        return;
    case kVkF4:  // seg11:0x7316 [BYTES seg11:0x7316]
        w.w913e = 2;  // the next 1500 ms tick calls the event routine seg18:0x5cec
        return;
    case kVkF6: {  // seg11:0x733b [BYTES seg11:0x733b-0x7359]
        c.st.w104a = 1;
        // `call seg5:0xdc; mov cx, 10; cwd; idiv cx`: rand() is 0 ... 32767, so 0 ... 9.
        const int code = c.h.rng().rand() % 10;
        c.h.crime(p, code);
        return;
    }
    case kVkF7:  // seg11:0x735c: weapons 22, 23 and 26, then sound 0x25 [BYTES seg11:0x735c-0x738e]
        c.h.load_weapon(p, 22);
        c.h.load_weapon(p, 23);
        c.h.load_weapon(p, 26);
        c.h.play(kSoundWeapons);
        return;
    case kVkF8:  // seg11:0x7391 [BYTES seg11:0x7391-0x73a6]
        c.god = c.god == 0 ? 1 : 0;  // `cmp [0x104c], 1; sbb; neg`
        c.h.score_redraw();          // dword DGROUP:0x5018 = -1
        return;
    case kVkF9:  // seg11:0x73a9: DGROUP:0x1048 = 0 [BYTES seg11:0x73a9]
        c.dg_1048 = 0;
        return;
    default:
        return;
    }
}

// ---- the program's keys

CheatState& cheat_state() {
    static CheatState s;
    return s;
}

CheatLinks& cheat_links() {
    static CheatLinks l;
    return l;
}

namespace {

class LinkedCheatHost final : public CheatHost {
public:
    World* w = nullptr;
    shell::Rng& rng() override { return shell::game_rng(); }
    std::int16_t call_in(int ship, int kind, int msg) override {
        return w ? call_in_program(*w, ship, kind, msg) : std::int16_t{0};
    }
    void play(int id) override {
        if (cheat_links().play) return cheat_links().play(id);
        if (callin_links().play) return callin_links().play(id);
        todo("seg10:0x744c for a cheat key", "seg10:0x744c");
    }
    void sound(int id, int pool, int rec, int mode) override {
        if (cheat_links().sound) return cheat_links().sound(id, pool, rec, mode);
        if (callin_links().sound) return callin_links().sound(id, pool, rec, mode);
        todo("seg10:0x754a for a cheat key", "seg10:0x754a");
    }
    void message(int recipient, int pool, int rec, int code) override {
        if (cheat_links().message) return cheat_links().message(recipient, pool, rec, code);
        if (callin_links().message) return callin_links().message(recipient, pool, rec, code);
        todo("seg3:0x6ec for a cheat key", "seg3:0x6ec");
    }
    void race_finish() override {
        if (cheat_links().race_finish) return cheat_links().race_finish();
        todo("the race finish of key '1' (races are out of M5 stage 4)", "seg4:0x2da2");
    }
    void crime(int ship, int code) override {
        if (cheat_links().crime) return cheat_links().crime(ship, code);
        todo("the crime routine of F6", "seg13:0xb6f2");
    }
    void load_weapon(int ship, int wp) override {
        if (w) load_weapon_program(*w, ship, wp);
    }
    void score_redraw() override {
        if (cheat_links().score_redraw) return cheat_links().score_redraw();
        todo("the score redraw DGROUP:0x5018 = -1 of a cheat key", "seg11:0x6f14");
    }
    void todo(const std::string& what, const char* address) override {
        if (callin_links().todo) return callin_links().todo(what, address);
        // Logged once per text through the guard facility.
        static std::set<std::string> seen;
        if (!seen.insert(what).second) return;
        bool once = false;
        guard_todo(once, what.c_str(), address);
    }
};

}  // namespace

CheatHost& linked_cheat_host(World& w) {
    static LinkedCheatHost h;
    h.w = &w;
    return h;
}

bool cheat_gate_program(World& w, std::int16_t& god, std::int16_t& dg_1048) {
    CheatCtx c{w, cheat_state(), god, dg_1048, linked_cheat_host(w)};
    return cheat_gate(c);
}

void cheat_key_program(World& w, std::int16_t& god, std::int16_t& dg_1048, std::uint16_t vk) {
    CheatCtx c{w, cheat_state(), god, dg_1048, linked_cheat_host(w)};
    cheat_key(c, vk);
}

}  // namespace is::world
