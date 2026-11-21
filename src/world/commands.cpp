// The commands to ships (commands.h). Each routine follows its listing; the readings are
// work/analysis/m5/s4-commands.md sections 0, 1, 1.9, 1.11 and 5 with s4-commands-verify.md (its
// CORRECTED verdicts and Omissions win), and work/world/m5s4/commands-report.md "Readings no
// verifier has checked" for seg8:0x9028 and the mover's state 15 as this track read them.
#include "world/commands.h"

#include <map>
#include <string>

#include "world/missile.h"
#include "world/pilot_orders.h"
#include "world/player_tick.h"
#include "world/relations.h"
#include "world/world.h"

namespace is::world {

namespace {

constexpr int kShips = 14;  // the literal 14 of the pool tests and of DGROUP:0x1e30 (pitfalls.md)

// One log per site [DESIGN ADR 0016 decision 7].
void log_site(const char* what, const char* address) {
    static std::map<std::string, bool> once;
    guard_todo(once[std::string(address) + what], what, address);
}

// ---- the words
// A ship record by index: outside the table it reads a zeroed scratch record and writes are lost
// [DESIGN ADR 0016 decision 2].
ShipRecord& ship(CommandCtx& c, int j, const char* address) {
    if (j >= 0 && j < kShipCount) return c.p.s(j);
    static ShipRecord zero;
    zero = ShipRecord{};
    log_site("a ship record past the eight-record table reads 0", address);
    return zero;
}
std::int16_t w(CommandCtx& c, int j, int off, const char* a = "seg13") { return ship_w(ship(c, j, a), off); }
void setw(CommandCtx& c, int j, int off, int v, const char* a = "seg13") { set_ship_w(ship(c, j, a), off, static_cast<std::int16_t>(v)); }
std::uint32_t d(CommandCtx& c, int j, int off, const char* a = "seg13") { return ship_d(ship(c, j, a), off); }
void setd(CommandCtx& c, int j, int off, std::uint32_t v, const char* a = "seg13") { set_ship_d(ship(c, j, a), off, v); }
std::int8_t sb(CommandCtx& c, int j, int off, const char* a = "seg13") { return ship_sb(ship(c, j, a), off); }
void setb(CommandCtx& c, int j, int off, std::uint8_t v, const char* a = "seg13") { set_ship_b(ship(c, j, a), off, v); }
// A byte j + base + k of ship j's record (the grudge +0xc2, the memory +0xee, the fight +0x13e
// bytes indexed by another ship or a record): an index past the record reads 0 [DESIGN ADR 0016
// decision 2].
std::int8_t sbk(CommandCtx& c, int j, int base, int k, const char* a) {
    const int off = base + k;
    if (off < 0 || off >= static_cast<int>(kShipRecordSize)) {
        log_site("a ship byte indexed past the record reads 0", a);
        return 0;
    }
    return sb(c, j, off, a);
}
void setbk(CommandCtx& c, int j, int base, int k, std::uint8_t v, const char* a) {
    const int off = base + k;
    if (off < 0 || off >= static_cast<int>(kShipRecordSize)) {
        log_site("a ship byte indexed past the record is not written", a);
        return;
    }
    setb(c, j, off, v, a);
}

std::uint32_t now(CommandCtx& c) { return c.p.g.now; }                      // DGROUP:0x1e50
int player(CommandCtx& c) { return c.p.g.player; }                          // DGROUP:0x9646
bool strategy(CommandCtx& c) { return c.p.g.play_mode != 0; }               // DGROUP:0x9134
bool player_game(CommandCtx& c) { return c.p.g.demo != 0; }                 // DGROUP:0x95bc
// DGROUP:0x9316 as a loop bound: past ship 7 the original's loops read other DGROUP blocks; the
// port stops at 7 [DESIGN ADR 0016 decision 2].
int last(CommandCtx& c) {
    if (c.p.g.last_ship < kShipCount) return c.p.g.last_ship;
    log_site("DGROUP:0x9316 past the ship table", "seg13:0x81c2");
    return kShipCount - 1;
}
// The pool-14 object `state` +0x18 through DGROUP:0x1e30 (the literal pool 14).
bool in_play(CommandCtx& c, int j) { return c.p.obj(kShips, j).state == 0; }
bool alive(CommandCtx& c, int j) { return ship_b(ship(c, j, "seg13:0x717c"), pw::kAlive) != 0; }  // byte +0xec
// The design words of ship j's design (+0x4e): +0x38 the team DGROUP:0x970a, +0x1e the tank
// DGROUP:0x96f0. A ship index past the table reads the zeroed record, design 0.
std::int16_t design(CommandCtx& c, int j, int off, const char* a) {
    return c.p.host.design_word(ship(c, j, a).design_index, off);
}
std::int16_t team(CommandCtx& c, int j, const char* a = "seg13:0x970a") { return design(c, j, 0x38, a); }
// tank / 10 > fuel +0x1e: `cdq; idiv 10` (16-bit) and `jle` [BYTES seg13:0x8458-0x8473].
bool low_fuel(CommandCtx& c, int j) { return idiv16(design(c, j, 0x1e, "seg13:0x96f0"), 10) > w(c, j, 0x1e); }

// AX of `mov al, a; imul byte b`: sext(a)·sext(b).
std::int16_t imul8(int a, std::int8_t b) { return static_cast<std::int16_t>(static_cast<std::int8_t>(a) * b); }
// now - sext16(lo16(2000·sbyte)) + 8000 (`cwde; imul ax, ax, 0x7d0; cdq; sub; sbb; add 0x1f40;
// adc 0`) [BYTES seg13:0x82e3-0x8304].
std::uint32_t goal_since(CommandCtx& c, std::int8_t b) {
    const std::int16_t v = mul16(b, 2000);
    return now(c) - static_cast<std::uint32_t>(static_cast<std::int32_t>(v)) + 8000u;
}
// +0x52 in {0, 7}.
bool order_free(CommandCtx& c, int j) { return w(c, j, pw::kOrder) == 0 || w(c, j, pw::kOrder) == 7; }
// The cooldown test: when c != P or DGROUP:0x9134 != 0, dword +0x9c > now refuses (`jb` on the
// high word, `ja`/`jbe` on the low) [BYTES seg13:0x81d5-0x8201].
bool cooling(CommandCtx& c, int asker) {
    if (asker == player(c) && !strategy(c)) return false;
    return d(c, asker, pw::kD9c) > now(c);
}

void msg(CommandCtx& c, int r, int p, int s, int code) { c.p.host.message(r, p, s, code); }  // seg3:0x6ec
void target(CommandCtx& c, int i, int p, int r) { c.p.host.target(i, p, r); }               // seg3:0xf4

// The request record (s4-commands.md 1.11): only when now - 15000 > dword DGROUP:0x8858
// (unsigned, `jbe` skips) [BYTES seg13:0x84bc-0x84cd].
bool request_window(CommandCtx& c) { return now(c) - 15000u > c.cw.req_time; }
void record_request(CommandCtx& c, int order, int pool, int rec, int by) {
    c.cw.req_order = static_cast<std::int16_t>(order);
    c.cw.req_pool = static_cast<std::int16_t>(pool);
    c.cw.req_rec = static_cast<std::int16_t>(rec);
    c.cw.req_by = static_cast<std::int16_t>(by);
    c.cw.req_time = now(c);
}

}  // namespace

// ---- the words' homes
CommandWords& command_words() {
    static CommandWords cw;
    return cw;
}
CommandWords::DefenderWords& CommandWords::def(int k) {
    if (k < 0) {
        static DefenderWords scratch;
        scratch = DefenderWords{};
        return scratch;
    }
    if (static_cast<std::size_t>(k) >= defender.size()) defender.resize(static_cast<std::size_t>(k) + 1);
    return defender[static_cast<std::size_t>(k)];
}
std::uint32_t& CommandWords::def_timer(int k) {
    if (k < 0) {
        static std::uint32_t scratch;
        scratch = 0;
        return scratch;
    }
    if (static_cast<std::size_t>(k) >= timer.size()) timer.resize(static_cast<std::size_t>(k) + 1);
    return timer[static_cast<std::size_t>(k)];
}

// ---- CommandHost defaults (ruling R3)
void CommandHost::capture_count(int ship, int v) {  // seg13:0x4eec through the program's missile links
    if (missile_links().capture_count) return missile_links().capture_count(ship, v);
    log_site("the capture count", "seg13:0x4eec");
}
void CommandHost::attitude_change(World& w, int a, int b, int dt, int ds, int prop, int kind) {
    ::is::world::attitude_change(w, a, b, dt, ds, prop, kind);  // seg13:0x2714 (relations.h)
}
void CommandHost::order_notice(int) { log_site("the Strategy order notice (M8)", "seg3:0x33b4"); }
std::vector<DefenderEntry>* CommandHost::defenders() {
    log_site("the defender table", "DGROUP:0x52fe");
    return nullptr;
}
void CommandHost::drop_pod(int, int) { log_site("the resource pack drop (death.h drop_pod)", "seg13:0x498e"); }

namespace {
DefenderEntry* entry(CommandCtx& c, int k) {
    std::vector<DefenderEntry>* t = c.h.defenders();
    if (!t || k < 0 || k >= static_cast<int>(t->size())) return nullptr;
    return &(*t)[static_cast<std::size_t>(k)];
}
int defender_count(CommandCtx& c) { return c.p.pools.in_use(2); }  // DGROUP:0x940a
}  // namespace

// ---- seg13:0x81c2(c), "help me" [BYTES seg13:0x81c2-0x856e]
std::int16_t help_command(CommandCtx& c, int asker) {
    const int P = player(c);
    if (cooling(c, asker)) return -1;                                      // [BYTES seg13:0x81d5-0x8201]
    setd(c, asker, pw::kD9c, now(c) + 5000u);                              // [BYTES seg13:0x8204-0x821a]
    c.h.capture_count(asker, 15);                                          // [BYTES seg13:0x821e-0x8228]
    for (int j = 0; j <= last(c); ++j) {                                   // `jg` exit [BYTES seg13:0x8346-0x834f]
        if (!in_play(c, j) || !alive(c, j) || j == asker) continue;        // [BYTES seg13:0x8353-0x8374]
        // Free, or given by the asker, or the player asking in a player game [BYTES seg13:0x8376-0x83a1]
        const bool free_j = order_free(c, j) || d(c, j, pw::kOrderUntil) < now(c) || w(c, j, pw::kOrderBy) == asker;
        if (!free_j && !(player_game(c) && asker == P)) continue;
        // The score [BYTES seg13:0x83a3-0x8473]
        std::int16_t v = add16(c.p.host.relation(asker, j), imul8(0x9c, sb(c, j, pw::kAllies)));
        if (w(c, j, 0x26) < 40) v = sub16(v, 100);
        if (team(c, j) == team(c, asker)) v = add16(v, 100);
        const bool j_hunts = w(c, j, pw::kGoalPool) == kShips && w(c, j, pw::kGoalRec) == asker && w(c, j, pw::kGoalAct) == 1;
        const bool c_hunts = w(c, asker, pw::kGoalPool) == kShips && w(c, asker, pw::kGoalRec) == j && w(c, asker, pw::kGoalAct) == 1;
        if (j_hunts || c_hunts) v = sub16(v, 800);
        if (team(c, j) == 0 && team(c, asker) == 2) v = add16(v, 350);
        if (low_fuel(c, asker)) v = sub16(v, 1000);
        if (v <= -150) continue;                                           // `cmp 0xff6a; jle` [BYTES seg13:0x8478]
        c.h.capture_count(asker, 20);                                      // [BYTES seg13:0x8481-0x84a0]
        if (asker == P) c.h.capture_count(j, 40);
        if (j == P && strategy(c) && player_game(c)) {                     // [BYTES seg13:0x84a3-0x8500]
            if (request_window(c)) {
                record_request(c, 1, kShips, asker, asker);
                msg(c, P, kShips, asker, 0x34);
            }
            continue;
        }
        // seg13:0x8233 [BYTES seg13:0x8233-0x8343]
        if (team(c, j) == 7) continue;
        if (w(c, j, pw::kOrder) != 0) msg(c, w(c, j, pw::kOrderBy), kShips, asker, 0x13);
        if (asker == P && c.p.g.play_mode == 1) c.h.order_notice(1);
        setd(c, j, pw::kReplanAt, now(c) + 200u);
        setw(c, j, pw::kOrder, 1);
        setw(c, j, pw::kOrderPool, kShips);
        setw(c, j, pw::kOrderRec, asker);
        setd(c, j, pw::kOrderUntil, now(c) + 15000u);
        setd(c, asker, pw::kD9c, d(c, j, pw::kOrderUntil));                // the asker's +0x9c = j's +0x98 [BYTES seg13:0x82ba-0x82c6]
        setw(c, j, pw::kOrderBy, asker);
        target(c, j, kShips, asker);
        setb(c, j, pw::kOneShot, 1);
        setd(c, j, pw::kGoalSince, goal_since(c, sb(c, j, pw::kAllies)));  // byte +0x5d here [BYTES seg13:0x82e3]
        msg(c, j, kShips, asker, 2);
        c.h.attitude_change(c.p.w, asker, j, 15, asker == P ? 15 : 0x23, 0, -1);  // [BYTES seg13:0x831a-0x833d]
        return 1;
    }
    // Nobody [BYTES seg13:0x8503-0x8565] ([bp-0xa] is 0 here)
    if (asker != P) {
        const std::int16_t r = imod16(static_cast<std::int16_t>(c.p.rng.rand()), 14);  // seg5:0xdc, `cdq; idiv 14`
        if (r >= sub16(12, c.p.g.detail)) {
            if (add16(c.p.w.w992, 20) >= c.p.w.budget) return 0;           // DGROUP:0x992 + 20 >= DGROUP:0x932a
            if (c.p.host.call_in(asker, 4, 0x68) != 0) return 0;           // seg13:0xadbe(c, 4, 0x68)
        }
    }
    if (asker == P && !player_game(c)) return 0;
    msg(c, asker, 7, 0, 6);
    return 0;
}

// ---- seg13:0x8570(c), "attack my target" [BYTES seg13:0x8570-0x8e95]
std::int16_t attack_command(CommandCtx& c, int asker) {
    const int P = player(c);
    if (cooling(c, asker)) return -1;                                      // [BYTES seg13:0x857e-0x85ac]
    setd(c, asker, pw::kD9c, now(c) + 10000u);                             // [BYTES seg13:0x85af-0x85c5]
    const bool p_in_game = asker == P && player_game(c);
    if (c.p.w.w934e < 5 && !p_in_game) return 0;                           // DGROUP:0x934e [BYTES seg13:0x85c9-0x85e1]
    const int tp = w(c, asker, pw::kTargetPool);                           // [bp-8]
    const int tr = w(c, asker, pw::kTargetRec);                            // [bp-0xa]
    if (!p_in_game && w(c, P, pw::kGoalPool) == tp) {                      // [BYTES seg13:0x85f7-0x8621]
        if (tp == 0x16) return 0;
        if (w(c, P, pw::kGoalRec) == tr) return 0;
    }
    if (asker != P && tp == 9) {                                           // [BYTES seg13:0x8623-0x86b1]
        if (c.cw.virus_ask != 0 && c.cw.virus_ask + 1500u > c.p.g.game_frames) return 0;
        if (player_tick_state().dg_8714 != 0 && player_tick_state().dg_8714 + 600u > c.p.g.game_frames) return 0;
        for (int j = 0; j < c.p.g.last_ship && j < kShipCount; ++j) {     // `jge`: j < DGROUP:0x9316
            if (j == P || !alive(c, j)) continue;
            if (w(c, j, pw::kGoalPool) == kShips && w(c, j, pw::kGoalRec) == P && w(c, j, pw::kGoalAct) == 1) return 0;
        }
        if (w(c, P, 0x26) < 40) return 0;
    }
    c.h.capture_count(asker, 4);                                           // [BYTES seg13:0x86b5-0x86bf]
    std::int16_t best = 1000;                                              // [bp-0x14]
    int who = -1;                                                          // [bp-0x16]
    bool who_set = false;
    const char* k8570 = "seg13:0x8570";
    for (int j = 0; j <= last(c); ++j) {                                   // [BYTES seg13:0x8ae5-0x8aee]
        std::int16_t r = 1000;                                             // [bp-0x12]
        const std::int16_t s = add16(c.p.host.strength(j, tr), 20);        // seg4:0x5d9c(j, tr) + 20 [bp-0xc]
        const std::int16_t a = c.p.host.relation(j, asker);                // [bp-0xe]
        const std::int16_t b = tp == kShips ? c.p.host.relation(j, tr) : std::int16_t{-400};  // [bp-0x18]
        const std::int16_t tj = team(c, j), tc = team(c, asker);
        // The first test that holds [BYTES seg13:0x86d4-0x8a97]
        if (j == asker || !in_play(c, j)) r = 0xa3;
        else if (tp == kShips && tr == j) r = 0x9e;
        else if (tj == 2 && tp == kShips) r = 0x93;
        else if (tj == 7) r = 0x96;
        else if (tj == 6) r = 0x95;
        else if (tj == 4 && tc != 4 && tp == kShips) r = 0x94;
        else if (a < -100 && tj != tc) r = tp == kShips ? 0x9d : 0xa2;
        else if (add16(b, 70) > a || b > 150) r = 0x91;
        // Ship tr's team whatever the pool [BYTES seg13:0x87e5-0x8817]
        else if (tj != team(c, tr, k8570) &&
                 (sb(c, j, pw::kAllies) >= 3 || (a < 100 && sb(c, j, pw::kClass) >= 3))) r = 0x9a;
        // The record (not the pool) compared with 14: ship 14 lies past the table [BYTES seg13:0x8821-0x88c0]
        else if (tr == 14 && team(c, 14, k8570) == tj) r = 0x98;
        else if (tr == 14 && team(c, 14, k8570) == tc &&
                 !(w(c, 14, pw::kGoalAct, k8570) == 1 && w(c, 14, pw::kGoalPool, k8570) == kShips &&
                   w(c, 14, pw::kGoalRec, k8570) == asker) &&
                 tj != 3) r = tj == tc ? 0x9b : 0x92;
        else if (tp == 0 && c.p.obj(0, tr).b53 == 1 && c.p.obj(0, tr).damage == 0 && tj != 3) r = 0x97;  // [BYTES seg13:0x88c3-0x88fa]
        else if (sbk(c, j, pw::kEe0, tr, k8570) == 8) r = 0x99;             // byte j+0xee+tr [BYTES seg13:0x88fd-0x890a]
        else if (tr == 14 && sbk(c, j, pw::kGrudge0, tr, k8570) == -1) r = 0x90;  // byte j+0xd0 [BYTES seg13:0x8914-0x8925]
        else {
            // [BYTES seg13:0x892d-0x8951]: lo16(3a), `cdq; and dx, 3; add; sar 2`
            const std::int16_t a3 = mul16(a, 3);
            const std::int16_t q = sar16(add16(a3, a3 < 0 ? 3 : 0), 2);
            if (add16(sub16(imul8(0x3e, sb(c, j, pw::kAllies)), q), 60) > s) r = 0x8f;
            else if (low_fuel(c, j)) r = 0x8b;                              // [BYTES seg13:0x8959-0x8974]
            else {
                // (sext(+0xce) + dword +0xdc) < sext(lo16((sbyte +0x5e - 2)·50)), 32-bit signed [BYTES seg13:0x897c-0x89bb]
                const std::int32_t money = static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(static_cast<std::int32_t>(w(c, j, pw::kResources))) + d(c, j, pw::kCredit));
                const std::int32_t lim = mul16(sub16(sb(c, j, pw::kClass), 2), 50);
                const std::int16_t g0 = w(c, j, pw::kGoalRec);
                if (money < lim && a < 200 && w(c, j, 0x26) < 50 && s < 80) r = 0x9c;
                else if (w(c, j, pw::kGoalAct) == 1 && w(c, j, pw::kGoalPool) == kShips &&
                         sbk(c, j, pw::kFight0, g0, k8570) > 0 &&
                         // sic: the goal pool compared with c [BYTES seg13:0x89e1-0x89f6]
                         !(w(c, j, pw::kGoalPool) == asker && w(c, j, pw::kOrder) >= 2 && w(c, j, pw::kOrder) <= 4) &&
                         !(tp == kShips && g0 == tr)) r = 0x8e;
                else {
                    const std::int16_t o = w(c, j, pw::kOrder);
                    if (o != 0 && o != 7 && o != 0x0d && d(c, j, pw::kOrderUntil) > now(c) && w(c, j, pw::kOrderBy) != asker) {
                        if (!(w(c, j, pw::kOrderPool) == kShips && w(c, j, pw::kOrderRec) == asker)) r = 0x8d;
                        else if (o == 1 || o == 5) r = 0x9f;
                        else if (o >= 2 || !(o > 4)) r = w(c, j, pw::kOrderBy) == j ? 0xa1 : 0xa0;  // always [BYTES seg13:0x8a70-0x8a97]
                    }
                    // Each busy code jumps straight to the choice [BYTES seg13:0x8a54 seg13:0x8a6e
                    // seg13:0x8a95 seg13:0x8a9c]; the cloak test is reached only from the not-busy
                    // tests with r = 1000, and its `cmp [bp-0x12], 0x3e8; je` skips it, so 0x8c never
                    // comes from this routine and the port has no code for it [BYTES seg13:0x8a9e-0x8ab7].
                }
            }
        }
        // The choice [BYTES seg13:0x8abc-0x8ae2]
        if (best > r) {
            best = r;
            who = j;
            who_set = true;
            continue;
        }
        if (best > 0x90 && r < 0x9e) {
            if ((c.p.rng.rand() & 1) != 0) {  // seg5:0xdc, `test al, 1` [BYTES seg13:0x8ad0-0x8ad7]
                best = r;
                who = j;
                who_set = true;
                continue;
            }
        }
        if (r != 1000) continue;  // [BYTES seg13:0x8b38-0x8b3d]
        // j obeys [BYTES seg13:0x8b3f-0x8d8e]
        c.h.capture_count(j, 15);
        if (asker == P) c.h.capture_count(j, 0x19);
        c.h.capture_count(asker, 0x14);
        if (tp == 9) {
            c.h.capture_count(j, -10);
            c.h.capture_count(asker, -13);
            if (asker != P) c.cw.virus_ask = c.p.g.game_frames;
        }
        if (j == P && strategy(c) && player_game(c)) {                     // [BYTES seg13:0x8b9e-0x8bfc]
            if (request_window(c)) {
                record_request(c, 4, tp, tr, asker);
                msg(c, P, kShips, asker, 0x34);
            }
            continue;
        }
        if (w(c, j, pw::kOrder) != 0) msg(c, w(c, j, pw::kOrderBy), kShips, asker, 0x13);  // [BYTES seg13:0x8bff-0x8c1b]
        if (asker == P) c.p.w.w9432 = add16(c.p.w.w9432, tp == kShips ? 120 : 30);    // DGROUP:0x9432 [BYTES seg13:0x8c1e-0x8c33]
        // The order by the allies byte; the store of 4 cannot be reached [BYTES seg13:0x8c38-0x8c6c]
        if (sb(c, j, pw::kAllies) >= 2) setw(c, j, pw::kOrder, 2);
        else if (sb(c, j, pw::kAllies) <= 1) setw(c, j, pw::kOrder, 3);
        else if (sb(c, j, pw::kAllies) == 0) setw(c, j, pw::kOrder, 4);
        ship_reaction(c.p, j, tp, tr, 9);                                  // seg4:0x8346(j, tp, tr, 9)
        if (j == P) setw(c, j, pw::kOrder, 2);
        if (asker == P && c.p.g.play_mode == 1) c.h.order_notice(2);
        setd(c, j, pw::kReplanAt, now(c) + 400u);
        setw(c, j, pw::kOrderPool, tp);
        setw(c, j, pw::kOrderRec, tr);
        setw(c, j, pw::kOrderBy, asker);
        setd(c, j, pw::kOrderUntil, now(c) + 10000u);
        setd(c, asker, pw::kD9c, now(c) + 15000u);
        setb(c, j, pw::kOneShot, 1);
        setd(c, j, pw::kGoalSince, goal_since(c, sb(c, j, pw::kClass)));  // byte +0x5e here [BYTES seg13:0x8d09]
        msg(c, j, kShips, asker, 2);
        if (j != P) target(c, j, tp, tr);
        if (tp == kShips) {  // seg13:0x2714(tr, c, -10·o, -40·o, 0, 5) [BYTES seg13:0x8d5a-0x8d83]
            const std::int16_t o = w(c, j, pw::kOrder);
            c.h.attitude_change(c.p.w, tr, asker, sub16(0, mul16(o, 10)), mul16(o, -40), 0, 5);
        }
        return 1;
    }
    // Nobody obeyed [BYTES seg13:0x8d91-0x8e8c]
    if (asker != P && order_free(c, P) && !strategy(c) && !(tp == kShips && tr == P) &&
        c.p.host.relation(asker, P) > -100 &&
        !(w(c, P, pw::kGoalPool) == kShips && w(c, P, pw::kGoalRec) == asker)) {
        setw(c, P, pw::kOrder, 2);
        setw(c, P, pw::kOrderBy, asker);
        setd(c, P, pw::kOrderUntil, now(c) + 15000u);
        setd(c, asker, pw::kD9c, d(c, P, pw::kOrderUntil));
        setw(c, P, pw::kOrderPool, tp);
        setw(c, P, pw::kOrderRec, tr);
        ship_reaction(c.p, P, tp, tr, 9);
        msg(c, P, kShips, asker, 2);
        if (tp == 9) c.cw.virus_ask = c.p.g.game_frames;
        return 1;
    }
    if (best == 0xa3) {
        msg(c, asker, 7, 0, 0xa3);
    } else {
        if (!who_set) {
            // [bp-0x16] is written only by a kept code: with no ship in the loop it is a stack word
            // [DESIGN ADR 0016 decision 1]: 0.
            log_site("seg13:0x8570's kept ship [bp-0x16] unwritten reads 0", "seg13:0x8e86");
            who = 0;
        }
        msg(c, asker, kShips, who, best);
    }
    return 0;
}

// ---- seg13:0x910a(c) [BYTES seg13:0x910a-0x915b]
std::int16_t no_other_ship(CommandCtx& c, int asker) {
    for (int j = 0; j <= last(c); ++j) {
        if (j == asker || !in_play(c, j) || !alive(c, j)) continue;
        return 0;
    }
    return 1;
}

// ---- seg13:0x8e98(c), "defend my target" [BYTES seg13:0x8e98-0x9109]
void defend_command(CommandCtx& c, int asker) {
    const int P = player(c);
    const int tp = w(c, asker, pw::kTargetPool), tr = w(c, asker, pw::kTargetRec);
    if (tp != kShips) return;                                              // [BYTES seg13:0x8ebf-0x8ec2]
    if (cooling(c, asker)) return;                                         // [BYTES seg13:0x8ec6-0x8eef]
    setd(c, asker, pw::kD9c, now(c) + 5000u);
    if (no_other_ship(c, asker) != 0) {                                    // [BYTES seg13:0x8f0d-0x8f1b]
        msg(c, asker, 7, 0, 0x64);
        return;
    }
    for (int j = 0; j <= last(c); ++j) {                                   // [BYTES seg13:0x8fe2-0x9099]
        if (team(c, j) == 6 || !in_play(c, j) || !alive(c, j)) continue;
        const bool busy = !order_free(c, j) && !(d(c, j, pw::kOrderUntil) < now(c));
        if (busy && asker != P) continue;
        const std::int16_t v = add16(add16(c.p.host.relation(j, tr), imul8(0xce, sb(c, j, pw::kAllies))),
                                     c.p.host.relation(j, asker));
        if (v <= 0 || j == asker || tr == j) continue;
        if (j == P && strategy(c) && player_game(c)) {                     // [BYTES seg13:0x9083-0x90d9]
            if (request_window(c)) {
                record_request(c, 5, tp, tr, asker);
                msg(c, P, kShips, asker, 0x34);
            }
            return;
        }
        // seg13:0x8f26 [BYTES seg13:0x8f26-0x8fdf]
        if (team(c, j) == 7) continue;
        setw(c, j, pw::kOrder, 5);
        msg(c, w(c, j, pw::kOrderBy), kShips, asker, 0x13);  // always, to the old +0x58
        setd(c, j, pw::kReplanAt, now(c) + 200u);
        setd(c, j, pw::kOrderUntil, now(c) + 20000u);
        setd(c, asker, pw::kD9c, now(c) + 15000u);
        setw(c, j, pw::kOrderPool, w(c, asker, pw::kTargetPool));
        setw(c, j, pw::kOrderRec, w(c, asker, pw::kTargetRec));
        setb(c, j, pw::kOneShot, 1);
        msg(c, j, kShips, asker, 2);
        setw(c, j, pw::kOrderBy, asker);
        c.h.attitude_change(c.p.w, w(c, asker, pw::kTargetRec), asker, 15, 50, 1, -1);
        return;
    }
    if (asker == P && !player_game(c)) return;                             // [BYTES seg13:0x90db-0x90f9]
    msg(c, asker, 7, 0, 5);
}

// ---- seg13:0x915c(c), "challenge my target" [BYTES seg13:0x915c-0x980f]
void challenge_command(CommandCtx& c, int asker) {
    const int P = player(c);
    const char* k915c = "seg13:0x915c";
    const int tp = w(c, asker, pw::kTargetPool), t = w(c, asker, pw::kTargetRec);  // [bp-6], [bp-8]
    std::int16_t q = 0;  // [bp-0x16]
    setd(c, asker, pw::kD9c, now(c) + 5000u);                              // first [BYTES seg13:0x9183-0x9195]
    if (tp != kShips) return msg(c, asker, 7, 0, 0x14);                    // [BYTES seg13:0x9199-0x91a4]
    if (team(c, t, k915c) == 7) return msg(c, asker, kShips, t, 0x40);
    if (low_fuel(c, t)) return msg(c, asker, kShips, t, 0xbc);
    if (w(c, t, pw::kOrder, k915c) == 1) return msg(c, asker, kShips, t, 0xbd);
    // The cooldown test, always true after the store above; skipped for the player in Action mode
    // or in a player game (verify claim 35) [BYTES seg13:0x91f1-0x922b]
    if (!(asker == P && (!strategy(c) || player_game(c)))) {
        if (d(c, asker, pw::kD9c) > now(c) && sbk(c, asker, pw::kGrudge0, t, k915c) < 4) return;
    }
    std::int16_t r = 0;                                                    // [bp-0x18]
    const std::int16_t s = c.p.host.strength(asker, t);                    // [bp-0x14]
    if (s > 500 || s < 20) {                                               // [BYTES seg13:0x9245-0x929e]
        if (sb(c, t, pw::kClass, k915c) > 0 && t != P) {
            if (s > 500) {
                msg(c, asker, kShips, t, 0x4c);
                help_command(c, t);
                return;
            }
            return msg(c, asker, kShips, t, 0x4e);
        }
        if (s < 20) r = 1;
        if (s > 500) r = 2;
    }
    // Both already fight each other on their own orders [BYTES seg13:0x92a3-0x92fa]
    const auto own_fight = [&](int x, int y) {
        const std::int16_t o = w(c, x, pw::kOrder, k915c);
        return w(c, x, pw::kOrderPool, k915c) == kShips && w(c, x, pw::kOrderRec, k915c) == y && o >= 2 && o <= 4 &&
               w(c, x, pw::kOrderBy, k915c) == x;
    };
    if (own_fight(t, asker) && own_fight(asker, t)) return msg(c, asker, kShips, t, 0x74);
    if (!in_play(c, t) || !alive(c, t)) return;                            // [BYTES seg13:0x92fd-0x9318]
    const std::int16_t dd = c.p.host.strength(asker, t);                   // [bp-0xc]
    const std::int16_t e = imul8(0x1e, sb(c, t, pw::kClass, k915c));       // [bp-0x10]
    const std::int16_t a = c.p.host.relation(t, asker);                    // [bp-0xa]
    const std::int8_t g = sbk(c, asker, pw::kGrudge0, t, k915c);
    std::int16_t wt = g > 2 ? imul8(0x1e, g) : std::int16_t{0};            // [bp-0xe] [BYTES seg13:0x9349-0x9363]
    const bool t_busy = !order_free(c, t) && d(c, t, pw::kOrderUntil, k915c) > now(c);
    if (a > 50) {                                                          // [BYTES seg13:0x9366-0x9420]
        wt = add16(wt, imul8(0x14, sb(c, t, pw::kAllies, k915c)));
        std::int16_t ab = sub16(100, dd);
        if (ab < 0) ab = sub16(0, ab);                                     // seg9:0x88ba, abs
        wt = add16(wt, sub16(idiv16(a, -5), add16(ab, ab)));
        wt = add16(wt, imul8(0x0f, g));
        if (g == -1) wt = sub16(wt, 100);
        if (r != 0) {
            q = 2;
        } else if (wt > -70 && (order_free(c, t) || d(c, t, pw::kOrderUntil, k915c) < now(c) || t == P)) {
            q = 2;
        } else if (!(asker == P && !player_game(c))) {
            const bool busy_msg = w(c, t, pw::kOrder, k915c) != 0 && d(c, t, pw::kOrderUntil, k915c) > now(c);
            msg(c, asker, kShips, t, busy_msg ? 0x10 : 0x0e);
        }
    } else {                                                               // [BYTES seg13:0x9423-0x94cc]
        wt = t_busy ? std::int16_t{-110} : std::int16_t{-30};
        wt = add16(wt, add16(sub16(sub16(imul8(0x19, g), idiv16(a, 5)), e), dd));
        if (g == -1) wt = sub16(wt, 140);
        if (r != 0 || wt > -90) {
            q = 1;
        } else if (!(asker == P && !player_game(c))) {
            msg(c, asker, kShips, t, t_busy ? 0x10 : 0x0f);
        }
    }
    if (q == 0) {                                                          // [BYTES seg13:0x94cf-0x94e2]
        setbk(c, asker, pw::kGrudge0, t, 0xff, k915c);
        return;
    }
    // Accepted [BYTES seg13:0x94e5-0x975b]
    setbk(c, asker, pw::kGrudge0, t, 0x15, k915c);
    setbk(c, t, pw::kGrudge0, asker, 0x14, k915c);
    if (w(c, t, pw::kOrder, k915c) != 0) msg(c, w(c, t, pw::kOrderBy, k915c), kShips, asker, 0x13);
    if (w(c, asker, pw::kOrder) != 0) msg(c, w(c, asker, pw::kOrderBy), kShips, asker, 0x13);
    if (t == P && strategy(c) && player_game(c)) {                         // [BYTES seg13:0x9535-0x957b]
        if (request_window(c)) record_request(c, 0x0b, kShips, asker, asker);
    } else {
        setw(c, t, pw::kOrder, 2, k915c);
        setd(c, t, pw::kReplanAt, now(c) + 200u, k915c);
        setd(c, t, pw::kOrderUntil, now(c) + 40000u, k915c);
        setd(c, t, pw::kD9c, now(c) + 8000u, k915c);
        setw(c, t, pw::kOrderPool, kShips, k915c);
        setw(c, t, pw::kOrderRec, asker, k915c);
        setw(c, t, pw::kOrderBy, t, k915c);
        setb(c, t, pw::kOneShot, 1, k915c);
        setw(c, t, pw::kGoalPool, kShips, k915c);
        setw(c, t, pw::kGoalRec, asker, k915c);
        setw(c, t, pw::kGoalAct, 1, k915c);
        setd(c, t, pw::kGoalSince, goal_since(c, sb(c, t, pw::kClass, k915c)), k915c);
        ship_reaction(c.p, t, kShips, asker, 0x15);
        if (t == P) c.p.host.set_manual_target();                          // DGROUP:0xa62 = 1
        setw(c, asker, pw::kOrder, 2);
        setd(c, asker, pw::kReplanAt, now(c) + 200u);
        setd(c, asker, pw::kOrderUntil, now(c) + 40000u);
        setd(c, asker, pw::kD9c, now(c) + 8000u);
        setw(c, asker, pw::kOrderPool, kShips);
        setw(c, asker, pw::kOrderRec, t);
        setw(c, asker, pw::kOrderBy, asker);
        setb(c, asker, pw::kOneShot, 0);
        setw(c, asker, pw::kGoalAct, 1);
        if (asker != P) target(c, asker, kShips, t);
        setw(c, asker, pw::kGoalPool, kShips);
        setw(c, asker, pw::kGoalRec, t);
        setd(c, asker, pw::kGoalSince, goal_since(c, sb(c, asker, pw::kClass)));
        if (q == 1 && (team(c, asker) == 4 || team(c, asker) == 0)) setw(c, asker, pw::kOrder, 3);  // [BYTES seg13:0x96c2-0x96e1]
        ship_reaction(c.p, asker, kShips, t, 0x14);
        if (c.p.g.wave_kind != 7) {                                        // the duel words, low words only [BYTES seg13:0x96f9-0x970b]
            ContactWords& cw = contact_words();
            cw.w9682 = 0;
            cw.w9680 = 0;
            cw.d8e04 &= 0xffff0000u;
            cw.d8e00 &= 0xffff0000u;
        }
        if (q != 0) {
            setbk(c, asker, pw::kFight0, t, 0, k915c);
            setbk(c, t, pw::kFight0, asker, 0, k915c);
            if (asker == P && c.p.g.play_mode == 1) c.h.order_notice(3);
        }
        c.h.attitude_change(c.p.w, w(c, asker, pw::kTargetRec), asker, -12, -55, 1, -1);  // [BYTES seg13:0x9742-0x975b]
    }
    // The replies [BYTES seg13:0x975e-0x97ff]
    if (r == 1) {
        msg(c, t, kShips, asker, 0x4b);
        msg(c, asker, kShips, t, 0x4d);
    } else if (r == 2) {
        msg(c, t, kShips, asker, 0x4d);
        msg(c, asker, kShips, t, 0x4b);
    } else if (w(c, asker, pw::kOrder) == 3) {
        msg(c, t, kShips, asker, 0x82);
        msg(c, asker, kShips, t, 0x12);
    } else if (q == 1) {
        msg(c, asker, kShips, t, 0x12);
        msg(c, t, kShips, asker, 0x16);
    } else if (q == 2) {
        msg(c, asker, kShips, t, 0x11);
        msg(c, t, kShips, asker, 0x15);
    }
}

// ---- seg13:0x9810(c), "self-preserve" [BYTES seg13:0x9810-0x996d]
void preserve_command(CommandCtx& c, int asker) {
    const char* k9810 = "seg13:0x9810";
    if (asker != player(c) && d(c, asker, pw::kD9c) > now(c)) return;     // [BYTES seg13:0x9823-0x9845]
    const int tp = w(c, asker, pw::kTargetPool), t = w(c, asker, pw::kTargetRec);
    if (no_other_ship(c, asker) != 0) return msg(c, asker, 7, 0, 0x64);
    if (tp != kShips) return;
    if (team(c, t, k9810) == 7) return msg(c, asker, kShips, t, 0x40);
    // [BYTES seg13:0x9890-0x98df]: `jae` on +0x98 for busy
    const bool busy = !order_free(c, t) && d(c, t, pw::kOrderUntil, k9810) >= now(c);
    if (!in_play(c, t) || !alive(c, t) || busy || t == asker || c.p.host.relation(t, asker) <= 0)
        return msg(c, asker, kShips, t, 0x5a);
    if (w(c, t, pw::kOrder, k9810) != 0) msg(c, w(c, t, pw::kOrderBy, k9810), kShips, t, 0x13);  // [BYTES seg13:0x98e1-0x98fd]
    setw(c, t, pw::kOrder, 0x0d, k9810);
    setd(c, t, pw::kReplanAt, now(c) + 200u, k9810);
    setw(c, t, pw::kOrderBy, asker, k9810);
    setd(c, t, pw::kOrderUntil, now(c) + 15000u, k9810);
    setb(c, t, pw::kOneShot, 0, k9810);
    msg(c, t, kShips, asker, 2);
}

// ---- seg13:0x9970(c) [BYTES seg13:0x9970-0x9a15]
std::int16_t ask_attack(CommandCtx& c, int asker) {
    if (d(c, asker, pw::kD9c) > now(c)) return -1;
    for (int j = 0; j <= last(c); ++j) {
        if (!in_play(c, j) || !alive(c, j)) continue;
        const std::int16_t o = w(c, j, pw::kOrder);
        if (o < 2 || o > 4 || w(c, j, pw::kOrderBy) != asker || j == player(c) || j == asker) continue;
        setd(c, asker, pw::kD9c, now(c) + 5000u);
        return 0;
    }
    return attack_command(c, asker);
}

// ---- seg13:0x9ce0(c), "capture my target" [BYTES seg13:0x9ce0-0x9f3c]
std::int16_t capture_command(CommandCtx& c, int asker) {
    const int P = player(c);
    if (cooling(c, asker)) return 0;
    const int tp = w(c, asker, pw::kTargetPool), tr = w(c, asker, pw::kTargetRec);
    const auto first_mate = [&]() {
        int j = 0;
        for (; j <= last(c); ++j)
            if (team(c, j) == team(c, asker) && j != asker) break;
        return j;
    };
    if (c.p.obj(tp, tr).b53 == 0) {                                        // [BYTES seg13:0x9d34-0x9dae]
        const int j = first_mate();
        if (j <= last(c)) msg(c, asker, kShips, j, tp == kShips ? 0x23 : 0x24);
        else msg(c, asker, 7, 0, 0x25);
        return 0;
    }
    for (int j = 0; j <= last(c); ++j) {                                   // [BYTES seg13:0x9db0-0x9edf]
        if (!in_play(c, j) || !alive(c, j)) continue;
        if (!(order_free(c, j) || d(c, j, pw::kOrderUntil) < now(c))) continue;  // `jae` skips
        if (j == asker || team(c, j) != team(c, asker)) continue;
        if (w(c, j, pw::kOrder) != 0) msg(c, w(c, j, pw::kOrderBy), kShips, j, 0x13);
        setw(c, j, pw::kOrder, 9);
        setd(c, j, pw::kReplanAt, now(c) + 200u);
        setw(c, j, pw::kOrderPool, tp);
        setw(c, j, pw::kOrderRec, tr);
        setw(c, j, pw::kOrderBy, asker);
        setd(c, j, pw::kOrderUntil, now(c) + 25000u);
        setd(c, asker, pw::kD9c, now(c) + 15000u);
        setb(c, j, pw::kOneShot, 1);
        msg(c, j, kShips, asker, 2);
        if (asker == P && c.p.g.play_mode == 1) c.h.order_notice(9);
        if (j != P) target(c, j, tp, tr);
        return 1;
    }
    const int j = first_mate();                                            // [BYTES seg13:0x9ee1-0x9f32]
    if (j <= last(c)) msg(c, asker, kShips, j, 0x26);
    else msg(c, asker, 7, 0, 0x27);
    return 0;
}

// ---- seg13:0x9f3e(c), "exit the wave" [BYTES seg13:0x9f3e-0xa01b]
void exit_command(CommandCtx& c, int asker) {
    const char* k9f3e = "seg13:0x9f3e";
    const int tp = w(c, asker, pw::kTargetPool), t = w(c, asker, pw::kTargetRec);
    if (tp != kShips || team(c, t, k9f3e) != team(c, asker)) return msg(c, asker, 7, 0, 0x32);
    const bool busy = !order_free(c, t) && d(c, t, pw::kOrderUntil, k9f3e) > now(c);
    if (busy && sb(c, t, pw::kAllies, k9f3e) > 1) return msg(c, asker, kShips, t, 0x31);
    if (team(c, t, k9f3e) == 7) return msg(c, asker, kShips, t, 0x40);
    if (w(c, t, pw::kOrder, k9f3e) != 0) msg(c, w(c, t, pw::kOrderBy, k9f3e), tp, t, 0x13);  // from (14, t)
    setw(c, t, pw::kOrder, 10, k9f3e);
    setd(c, t, pw::kReplanAt, now(c), k9f3e);
    msg(c, t, kShips, asker, 2);
}

// ---- seg13:0xa01c(c), "get gas" [BYTES seg13:0xa01c-0xa185]
void gas_command(CommandCtx& c, int asker) {
    const char* ka01c = "seg13:0xa01c";
    const int tp = w(c, asker, pw::kTargetPool), t = w(c, asker, pw::kTargetRec);
    if (tp == kShips && team(c, t, ka01c) == 7) return msg(c, asker, kShips, t, 0x40);
    if (tp != kShips || team(c, t, ka01c) != team(c, asker)) return msg(c, asker, 7, 0, 0x36);
    // dword +0x9c, sic (not +0x98) [BYTES seg13:0xa08a-0xa0b5]
    if (!order_free(c, t) && d(c, t, pw::kD9c, ka01c) > now(c) && sb(c, t, pw::kAllies, ka01c) > 0)
        return msg(c, asker, kShips, t, 0x35);
    if (w(c, t, pw::kOrder, ka01c) != 0) msg(c, w(c, t, pw::kOrderBy, ka01c), kShips, t, 0x13);
    setw(c, t, pw::kOrder, 0x0c, ka01c);
    setw(c, t, pw::kGoalPool, kShips, ka01c);
    setw(c, t, pw::kGoalRec, asker, ka01c);
    setw(c, t, pw::kGoalAct, 2, ka01c);
    setw(c, t, pw::kGoalApp, 2, ka01c);
    setw(c, t, pw::kOrderPool, kShips, ka01c);
    setw(c, t, pw::kOrderRec, asker, ka01c);
    setw(c, t, pw::kOrderBy, asker, ka01c);
    setd(c, t, pw::kOrderUntil, now(c) + 25000u, ka01c);
    setd(c, t, pw::kReplanAt, now(c) + 500u, ka01c);
    setd(c, asker, pw::kD9c, now(c) + 15000u);
    setb(c, t, pw::kOneShot, 1, ka01c);
    setd(c, t, pw::kGoalSince, goal_since(c, sb(c, t, pw::kClass, ka01c)), ka01c);
    msg(c, t, kShips, asker, 2);
}

// ---- the defenders
// seg8:0x9028(k, s, pool, rec), cdecl [BYTES seg8:0x9028-0x90f8]: no change when the owner is a
// ship (+0x06 = 14) and the entry is in state 15 or is told to attack its own owner ship, when
// the target is the owner itself, or when the target is pool-2 entry rec of the same owner; state
// 5 clears word +0x02 and raises the state to 5; any other state raises it to s (max), and 15
// clears the homing flag; the target words take (pool, rec).
void defender_order(CommandCtx& c, int k, int s, int pool, int rec) {
    DefenderEntry* e = entry(c, k);
    if (!e) return;
    if (e->owner_pool == kShips) {
        if (e->w00 == 15) return;
        if (pool == kShips && e->owner_rec == rec) return;
    }
    if (e->owner_pool == pool && e->owner_rec == rec) return;
    if (pool == 2) {
        if (const DefenderEntry* o = entry(c, rec); o) {
            if (o->owner_pool == e->owner_pool && o->owner_rec == e->owner_rec) return;
        } else {
            // DGROUP:0x5304 + 0x24·rec past the table [DESIGN ADR 0016 decision 2]: reads 0
            log_site("seg8:0x9028's pool-2 entry past the defender table reads 0", "seg8:0x907e");
            if (e->owner_pool == 0 && e->owner_rec == 0) return;
        }
    }
    if (s == 5) {
        c.cw.def(k).w02 = 0;
        if (e->w00 <= 5) e->w00 = 5;
    } else {
        if (!(e->w00 >= s)) e->w00 = static_cast<std::int16_t>(s);
    }
    e->killer_pool = static_cast<std::int16_t>(pool);
    e->killer_rec = static_cast<std::int16_t>(rec);
    if (s != 5 && s == 15) c.cw.def(k).homing = 0;
}

// seg13:0x9a18(i, pool, rec) [BYTES seg13:0x9a18-0x9ae6]
void send_defenders(CommandCtx& c, int i, int pool, int rec) {
    if (rec < 0) return;
    if (pool < 0 || pool >= kPoolCount) {
        log_site("seg13:0x9a18's pool past the 21 counts", "seg13:0x9a34");
        return;
    }
    if (!(c.p.pools.in_use(pool) > rec)) return;                           // `jle` [BYTES seg13:0x9a2f-0x9a38]
    if (c.p.obj(pool, rec).state != 0) return;
    int lastk = -1;  // [bp-8]
    for (int k = 0; k < defender_count(c); ++k) {
        DefenderEntry* e = entry(c, k);
        if (!e) break;
        if (e->owner_pool != kShips || e->owner_rec != i) continue;
        if (e->killer_pool == pool && e->killer_rec == rec && e->w00 == 15) continue;
        if (c.p.obj(2, k).state != 0) continue;                            // DGROUP:0x1e00
        e->w00 = 15;
        e->killer_pool = static_cast<std::int16_t>(pool);
        e->killer_rec = static_cast<std::int16_t>(rec);
        c.cw.def(k).homing = 0;                                            // DGROUP:0x52f6 + 0x24k [BYTES seg13:0x9ab1]
        lastk = k;
    }
    if (lastk >= 0) c.p.host.sound(0x15, 2, lastk, 1);                     // seg10:0x754a(0x15, 2, k, 1)
}

// seg13:0x9ae8(i) [BYTES seg13:0x9ae8-0x9bb7]
void defenders_return(CommandCtx& c, int i) {
    int lastk = -1;  // [bp-6], with k = [bp-4] = 0 from the one dword store
    for (int k = 0; k < defender_count(c); ++k) {
        DefenderEntry* e = entry(c, k);
        if (!e) break;
        if (e->owner_pool != kShips || e->owner_rec != i || e->w00 != 15) continue;
        e->w00 = 0;
        const ObjectRecord& owner = c.p.obj(e->owner_pool, e->owner_rec);  // DGROUP:0x1df8 + 4·(+0x06)
        ObjectRecord& def = c.p.obj(2, k);                                 // DGROUP:0x1e00
        // -(owner - defender): the defender's offset from its owner, per axis [BYTES seg13:0x9b23-0x9b7b]
        c.cw.def(k).dx = sub16(def.x, owner.x);
        c.cw.def(k).dy = sub16(def.y, owner.y);
        def.damage = 0;                                                    // object word +0x1e [BYTES seg13:0x9b7f]
        lastk = k;
    }
    if (lastk >= 0) c.p.host.sound(0x15, 2, lastk, 1);
}

// seg11:0x70bb-0x7113, Alt+E
void defenders_leave(CommandCtx& c) {
    int lastk = -1;  // [bp-0x16]
    for (int k = 0; k < defender_count(c); ++k) {
        DefenderEntry* e = entry(c, k);
        if (!e) break;
        if (e->owner_pool != c.p.g.ship_pool || e->owner_rec != player(c)) continue;  // DGROUP:0x9324
        defender_order(c, k, 15, 12, 1);
        lastk = k;
    }
    if (lastk >= 0) c.p.host.sound(0x15, 2, lastk, 1);
}

// The pool-2 mover's state 15 [BYTES seg8:0x8f5b-0x9000 seg8:0x8300-0x847e]
bool defender_attack_step(CommandCtx& c, int k, bool steer_frame) {
    DefenderEntry* e = entry(c, k);
    if (!e || e->w00 != 15) return false;
    // A target record past the pool's allocated count DGROUP:0x92ec + 2·pool (`jge` passes
    // equality) [BYTES seg8:0x8f68-0x8f7c]
    const int tp = e->killer_pool;
    const std::int16_t alloc = tp >= 0 && tp < kPoolCount ? c.p.pools.allocated(tp) : std::int16_t{0};
    if (!(alloc >= e->killer_rec)) {
        c.p.pools.bad_index_count = add16(c.p.pools.bad_index_count, 1);  // DGROUP:0x9498
        e->killer_pool = 12;
        e->killer_rec = 1;
    }
    if (c.p.obj(e->killer_pool, e->killer_rec).state != 0) {              // [BYTES seg8:0x8f85-0x8fb0]
        e->killer_pool = 12;
        e->killer_rec = 1;
    }
    ObjectRecord& o = c.p.obj(2, k);
    const std::uint32_t t = c.cw.def_timer(k);                             // dword +0x0e DGROUP:0x530c
    if (!steer_frame || !(t < now(c))) {                                   // [BYTES seg8:0x8fb4-0x8fd6]
        // The spin while the timer runs [BYTES seg8:0x844e-0x847e]
        if (t > now(c)) {
            o.frame = add16(o.frame, 1);
            if (o.frame > 15) o.frame = 0;
        }
        return true;
    }
    // The aim: a cloaked ship target's guess +0x14a / +0x14c [BYTES seg8:0x8fda-0x9000]
    std::int16_t tx = 0, ty = 0;
    if (e->killer_pool == kShips && ship_b(ship(c, e->killer_rec, "seg8:0x8fe3"), pw::kCloaked) != 0) {
        tx = w(c, e->killer_rec, pw::kGuessX, "seg8:0x8ff2");
        ty = w(c, e->killer_rec, pw::kGuessY, "seg8:0x8ff9");
    } else {
        const ObjectRecord& tg = c.p.obj(e->killer_pool, e->killer_rec);
        tx = tg.x;
        ty = tg.y;
    }
    // seg8:0x285a(defender, target): [bp-0x36] = target - defender in x, [bp-0x38] in y
    std::int16_t ox = 0, oy = 0;
    std::int16_t dist = cells_between(o, tx, ty, ox, oy);                  // [BYTES seg8:0x8325-0x836f]
    if (dist < 4) dist = 4;
    if (dist == 0) dist = 1;
    CommandWords::DefenderWords& dw = c.cw.def(k);
    if (dw.homing == 0) {
        // -100·o / d into [bp-6] / [bp-4], which nothing reads [BYTES seg8:0x8386-0x8399]
        if (dist > 220) dw.homing = 1;                                    // [BYTES seg8:0x839c-0x83a5]
        return true;
    }
    const std::int16_t q = sar16(dist, 2);
    const std::int16_t ax = idiv16(shl16(ox, 3), q);                       // [BYTES seg8:0x83ae-0x83cd]
    const std::int16_t ay = idiv16(shl16(oy, 3), q);
    o.vx = add16(o.vx, ax);
    o.vy = add16(o.vy, ay);
    o.frame = direction16(ay, ax);                                         // seg8:0x19b0(den = ax, num = ay)
    if (o.vx > 70) o.vx = 70;                                              // [BYTES seg8:0x83fc-0x8446]
    if (o.vy > 70) o.vy = 70;
    if (o.vx < -70) o.vx = -70;
    if (o.vy < -70) o.vy = -70;
    return true;
}

// The pool-2 mover's head [BYTES seg8:0x8277-0x82c0]
bool defender_mover_head(CommandCtx& c) {
    c.cw.steer = 0;
    if (!(c.cw.steer_due >= now(c))) {                                     // `jae` skips [BYTES seg8:0x827d-0x8286]
        std::int16_t t = shl16(sub16(0x37, c.p.w.wave), 2);                // (55 - DGROUP:0x91f8) << 2
        if (t < 0x6e) t = 0x6e;
        c.cw.steer_due = now(c) + static_cast<std::uint32_t>(static_cast<std::int32_t>(t));  // `cdq; add; adc`
        c.cw.steer = 1;
    }
    if (c.cw.w1874 >= 4) c.cw.w1874 = 0;                                   // [BYTES seg8:0x82b3-0x82c2]
    else c.cw.w1874 = add16(c.cw.w1874, 1);
    return c.cw.steer != 0;
}

// ---- seg13:0x4ac4(s), Ctrl+Alt+S [BYTES seg13:0x4ac4-0x4b9e]
void arrest_command(CommandCtx& c, int s) {
    c.h.drop_pod(s, 0);                                                    // seg13:0x498e(s, 0)
    for (int k = 0; k <= last(c); ++k) {
        if (!in_play(c, k) || k == s) continue;
        if (w(c, k, pw::kGoalAct) != 1 || w(c, k, pw::kGoalPool) != kShips || w(c, k, pw::kGoalRec) != s) continue;
        if (s >= 0 && s < kShipCount) c.p.w.w8680[static_cast<std::size_t>(s)] = static_cast<std::int16_t>(k);  // DGROUP:0x8680 + 2s
        if (ship(c, k, "seg13:0x4b42").roster_index != -1) {               // [BYTES seg13:0x4ae5-0x4b05]
            setw(c, s, pw::kGoalAct, 2, "seg13:0x4af6");
            setw(c, s, pw::kGoalPool, 0x0b, "seg13:0x4afc");               // the dword 0x0000000b
            setw(c, s, pw::kGoalRec, 0, "seg13:0x4afc");
            continue;
        }
        setw(c, k, pw::kOrder, 0x16);                                      // [BYTES seg13:0x4b54-0x4b83]
        setw(c, k, pw::kOrderPool, kShips);
        setw(c, k, pw::kOrderRec, s);
        setd(c, k, pw::kOrderUntil, now(c) + 50000u);
        setb(c, k, pw::kOneShot, 1);
        setd(c, k, pw::kReplanAt, now(c) + 500u);
        if (s == player(c)) {                                              // crime 5 recorded (law.h's homes)
            contact_words().ba163 = 5;                                     // byte DGROUP:0xa163
            c.p.w.ba14d[5] = static_cast<std::uint8_t>(c.p.w.ba14d[5] + 1);  // byte DGROUP:0xa152, no cap
        }
        return;
    }
}

// ---- the request record (s4-commands.md 1.11)
// seg13:0xa5c2() [BYTES seg13:0xa5c2-0xa659]
void request_answered(CommandCtx& c) {
    const int P = player(c);
    const CommandWords& r = c.cw;
    bool hit = false;
    if (w(c, P, pw::kOrder) == r.req_order && w(c, P, pw::kOrderPool) == r.req_pool && r.req_time + 20000u > now(c)) {
        // the record or, `cmp ax, 9` on DGROUP:0x9650, a virus; a miss here ends the routine
        // [BYTES seg13:0xa5f7-0xa604 seg13:0xa63b]
        hit = w(c, P, pw::kOrderRec) == r.req_rec || r.req_pool == 9;
    } else if (w(c, P, pw::kOrder) == 4 && r.req_order == 0x0b && r.req_time + 20000u > now(c) &&
               w(c, P, pw::kOrderPool) == r.req_pool && w(c, P, pw::kOrderRec) == r.req_rec) {
        hit = true;  // [BYTES seg13:0xa606-0xa63b]
    }
    if (!hit) return;
    c.cw.reply_due = now(c) + 700u;
    c.cw.req_time = now(c);
}

// seg13:0xa65a() [BYTES seg13:0xa65a-0xa6ba]
void request_reply(CommandCtx& c) {
    if (c.cw.reply_due == 0 || !(c.cw.reply_due < now(c))) return;
    // The dword DGROUP:0x8858 (a time, just set to now by seg13:0xa5c2) compared with 11: the duel
    // reply 0x1f runs only within the clock's first 11 ms (pitfalls.md) [BYTES seg13:0xa677-0xa67d]
    if (c.cw.req_time != 0x0b)
        bidding_report(c.p, c.cw.req_by);                                  // seg13:0x7b40(DGROUP:0x9654)
    else
        msg(c, player(c), c.cw.req_pool, c.cw.req_rec, 0x1f);
    c.cw.req_order = c.cw.req_pool = c.cw.req_rec = c.cw.req_by = 0;
    c.cw.reply_due = 0;
    c.cw.req_time = 0;
}

// ---- the Strategy keys (s4-commands.md 1.11 with the verifier's Omissions; the mark is the
// words DGROUP:0x6e54 / 0x6e52, PilotHost::mark_6e52)
void strategy_attack(CommandCtx& c) {  // seg13:0xa186 [BYTES seg13:0xa186-0xa232]
    if (!strategy(c)) return;
    const int P = player(c);
    std::int16_t mp = 0, mr = 0;
    c.p.host.mark_6e52(mp, mr);
    setw(c, P, pw::kOrder, 4);
    setw(c, P, pw::kOrderPool, mp);
    setw(c, P, pw::kOrderRec, mr);
    if (w(c, P, pw::kOrderPool) == 12) setw(c, P, pw::kOrderPool, 8);
    setd(c, P, pw::kOrderUntil, now(c) + 30000u);
    setd(c, P, pw::kReplanAt, now(c) + 200u);
    setb(c, P, pw::kOneShot, 1);
    setw(c, P, pw::kOrderBy, P);
    ship_reaction(c.p, P, mp, mr, 12);
    if (mp == kShips) c.h.attitude_change(c.p.w, mr, P, -15, -60, 1, 1);
    request_answered(c);
}

void strategy_capture(CommandCtx& c) {  // seg13:0xa234 [BYTES seg13:0xa234-0xa3c6]
    if (!strategy(c)) return;
    const int P = player(c);
    std::int16_t mp = 0, mr = 0;
    c.p.host.mark_6e52(mp, mr);
    int pool = mp, rec = mr;                                               // [bp-6], [bp-8]
    if (!((mp == 0 || mp == 0x0b) && c.p.obj(mp, mr).state == 0)) {
        std::int16_t best = 20000;                                         // [bp-0xc]
        int kind = -1, brec = 0;                                           // [bp-0xe], [bp-0x10]
        const ObjectRecord& me = c.p.obj(kShips, P);
        for (int k = 0; k < c.p.w.icons_a; ++k) {                          // DGROUP:0x91fa
            if (c.p.obj(0, k).state != 0) continue;
            std::int16_t dx = 0, dy = 0;
            const std::int16_t dd = cells_between(me, c.p.obj(0, k).x, c.p.obj(0, k).y, dx, dy);
            if (!(dd < best)) continue;
            best = dd;
            kind = 0;
            brec = k;
        }
        for (int k = 0; k < c.p.pools.in_use(11); ++k) {                  // DGROUP:0x941c
            if (c.p.obj(11, k).state != 0) continue;
            std::int16_t dx = 0, dy = 0;
            const std::int16_t dd = cells_between(me, c.p.obj(11, k).x, c.p.obj(11, k).y, dx, dy);
            if (!(dd < best)) continue;
            best = dd;
            kind = 0x0b;
            brec = k;
        }
        if (kind == -1) return msg(c, P, 7, 0, 0x25);
        target(c, P, kind, brec);
        pool = kind;
        rec = brec;
    }
    setw(c, P, pw::kOrder, 9);
    setd(c, P, pw::kReplanAt, now(c) + 200u);
    setw(c, P, pw::kOrderPool, pool);
    setw(c, P, pw::kOrderRec, rec);
    setw(c, P, pw::kOrderBy, P);
    setd(c, P, pw::kOrderUntil, now(c) + 25000u);
    setb(c, P, pw::kOneShot, 1);
    request_answered(c);
}

void strategy_exit(CommandCtx& c) {  // seg13:0xa3c8 [BYTES seg13:0xa3c8-0xa47b]
    if (!strategy(c)) return;
    const int P = player(c);
    std::int16_t best = 20000;
    const ObjectRecord& me = c.p.obj(kShips, P);
    for (int k = 0; k < 2 * c.p.g.gates; k += 2) {                         // DGROUP:0x9318
        std::int16_t dx = 0, dy = 0;
        const std::int16_t dd = cells_between(me, c.p.obj(12, k).x, c.p.obj(12, k).y, dx, dy);
        if (!(dd < best)) continue;
        best = dd;
        setw(c, P, pw::kOrder, 0x0a);                                      // the dword 0x000c000a
        setw(c, P, pw::kOrderPool, 0x0c);
        setw(c, P, pw::kOrderRec, k);
        setd(c, P, pw::kOrderUntil, now(c) + 30000u);
        setd(c, P, pw::kReplanAt, now(c) + 200u);
        setb(c, P, pw::kOneShot, 1);
        setw(c, P, pw::kOrderBy, P);
    }
    request_answered(c);
}

void strategy_gas(CommandCtx& c) {  // seg13:0xa47c [BYTES seg13:0xa47c-0xa4f3]
    if (!strategy(c)) return;
    const int P = player(c);
    if (c.p.pools.in_use(4) < 1) return msg(c, P, 7, 0, 0x38);             // DGROUP:0x940e
    setw(c, P, pw::kOrder, 0x0c);                                          // the dword 0x000e000c
    setw(c, P, pw::kOrderPool, kShips);
    setw(c, P, pw::kOrderRec, P);
    setd(c, P, pw::kOrderUntil, now(c) + 30000u);
    setd(c, P, pw::kReplanAt, now(c) + 200u);
    setb(c, P, pw::kOneShot, 1);
    setw(c, P, pw::kOrderBy, P);
    request_answered(c);
}

void strategy_help(CommandCtx& c) {  // seg13:0xa4f4 [BYTES seg13:0xa4f4-0xa567]
    if (!strategy(c)) return;
    const int P = player(c);
    std::int16_t mp = 0, mr = 0;
    c.p.host.mark_6e52(mp, mr);
    if (mp != kShips) return;
    setw(c, P, pw::kOrder, 1);
    setw(c, P, pw::kOrderPool, mp);
    setw(c, P, pw::kOrderRec, mr);
    setd(c, P, pw::kOrderUntil, now(c) + 30000u);
    setd(c, P, pw::kReplanAt, now(c) + 200u);
    setb(c, P, pw::kOneShot, 1);
    setw(c, P, pw::kOrderBy, P);
    request_answered(c);
}

void strategy_needs(CommandCtx& c) {  // seg13:0xa568 [BYTES seg13:0xa568-0xa5c0]: no seg13:0xa5c2
    if (!strategy(c)) return;
    const int P = player(c);
    setw(c, P, pw::kOrder, 0x0d);                                          // the dword 0x000e000d
    setw(c, P, pw::kOrderPool, kShips);
    setw(c, P, pw::kOrderRec, P);
    setd(c, P, pw::kOrderUntil, now(c) + 30000u);
    setd(c, P, pw::kReplanAt, now(c) + 200u);
    setb(c, P, pw::kOneShot, 1);
    setw(c, P, pw::kOrderBy, P);
}

// ---- the keys [BYTES seg11:0x6f20-0x7130] (verify claim 6, CORRECTED, and its Omissions)
void key_a(CommandCtx& c, bool ctrl, bool alt) {
    const int P = player(c);
    if (ctrl) attack_command(c, P);                                        // seg11:0x706f
    if (alt) {                                                             // seg11:0x707b: push dword +0x3e
        send_defenders(c, P, w(c, P, pw::kTargetPool), w(c, P, pw::kTargetRec));
        return;
    }
    if (!ctrl) strategy_attack(c);                                         // seg11:0x709d
}
void key_c(CommandCtx& c, bool ctrl, bool) {
    if (ctrl) capture_command(c, player(c));                               // seg11:0x7023
    else strategy_capture(c);                                              // seg11:0x702b
}
void key_d(CommandCtx& c, bool ctrl, bool) {
    if (ctrl) defend_command(c, player(c));                                // seg11:0x6f2a
    else strategy_help(c);                                                 // seg11:0x6f32
}
void key_e(CommandCtx& c, bool ctrl, bool alt) {
    if (ctrl) exit_command(c, player(c));                                  // seg11:0x70af
    if (alt) {                                                             // seg11:0x70bb-0x7113
        defenders_leave(c);
        return;
    }
    if (!ctrl) strategy_exit(c);                                           // seg11:0x711f
}
void key_f(CommandCtx& c, bool ctrl, bool) {
    if (ctrl) challenge_command(c, player(c));                             // seg11:0x6ff7
}
void key_g(CommandCtx& c, bool ctrl, bool) {
    if (ctrl) gas_command(c, player(c));                                   // seg11:0x7009
    else strategy_gas(c);                                                  // seg11:0x7011
}
void key_s(CommandCtx& c, bool ctrl, bool alt) {
    if (ctrl && alt) arrest_command(c, player(c));                         // seg11:0x7043
    else if (ctrl) preserve_command(c, player(c));                         // seg11:0x7055
    else strategy_needs(c);                                                // seg11:0x705d
}

// ---- the program's entry points
CommandLinks& command_links() {
    static CommandLinks l;
    return l;
}

namespace {
void run(const char* what, const char* address, const std::function<void(CommandCtx&)>& f) {
    if (command_links().with_ctx) return command_links().with_ctx(f);
    log_site(what, address);
}
}  // namespace

void command_key_a(bool ctrl, bool alt) { run("the A key", "seg11:0x7065", [&](CommandCtx& c) { key_a(c, ctrl, alt); }); }
void command_key_c(bool ctrl, bool alt) { run("the C key", "seg11:0x7019", [&](CommandCtx& c) { key_c(c, ctrl, alt); }); }
void command_key_d(bool ctrl, bool alt) { run("the D key", "seg11:0x6f20", [&](CommandCtx& c) { key_d(c, ctrl, alt); }); }
void command_key_e(bool ctrl, bool alt) { run("the E key", "seg11:0x70a5", [&](CommandCtx& c) { key_e(c, ctrl, alt); }); }
void command_key_f(bool ctrl, bool alt) { run("the F key", "seg11:0x6feb", [&](CommandCtx& c) { key_f(c, ctrl, alt); }); }
void command_key_g(bool ctrl, bool alt) { run("the G key", "seg11:0x6fff", [&](CommandCtx& c) { key_g(c, ctrl, alt); }); }
void command_key_s(bool ctrl, bool alt) { run("the S key", "seg11:0x7033", [&](CommandCtx& c) { key_s(c, ctrl, alt); }); }
void command_ctrl_h(int s) { run("Ctrl+H", "seg13:0x81c2", [&](CommandCtx& c) { help_command(c, s); }); }
void command_alt_h(int s) { run("Alt+H", "seg13:0x9ae8", [&](CommandCtx& c) { defenders_return(c, s); }); }
void command_challenge(int s) { run("the challenge", "seg13:0x915c", [&](CommandCtx& c) { challenge_command(c, s); }); }
void command_capture(int s) { run("the capture command", "seg13:0x9ce0", [&](CommandCtx& c) { capture_command(c, s); }); }
void command_defenders_return(int s) { run("the defenders' return", "seg13:0x9ae8", [&](CommandCtx& c) { defenders_return(c, s); }); }
void command_request_reply() { run("the command report", "seg13:0xa65a", [&](CommandCtx& c) { request_reply(c); }); }
void command_send_defenders(int i, int pool, int rec) {
    run("the defenders' attack", "seg13:0x9a18", [&](CommandCtx& c) { send_defenders(c, i, pool, rec); });
}

}  // namespace is::world
