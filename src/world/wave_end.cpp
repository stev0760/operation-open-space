// The wave's end (wave_end.h). 16-bit arithmetic as the original's (ADR 0013 decision 7).
#include "world/wave_end.h"

namespace is::world {

namespace {

// `cmp dword [d], 0; je skip; cmp dword [d], now; ja skip`: armed and not above now [BYTES
// seg7:0x5ce seg7:0x5da seg7:0x610 seg7:0x622 seg7:0x6ac seg7:0x6b8].
bool due(std::uint32_t d, std::uint32_t now) { return d != 0 && !(d > now); }

bool ship_ok(int i) { return i >= 0 && i < kShipCount; }

}  // namespace

DeadlineResult wave_deadlines(WaveEndCtx& c) {
    World& w = c.w;
    shell::GameState& s = c.s;
    const std::uint32_t now = static_cast<std::uint32_t>(s.now());
    // DGROUP:0x1e88, the ambulance [BYTES seg7:0x5ce-0x60b] [OPEN M6: its writer seg16:0x568 is
    // not ported, so it stays 0].
    if (due(w.w1e88, now)) {
        w.w1e88 = 0;  // [BYTES seg7:0x5e1]
        if (w.player == w.w952e && w.w951a == 0) {
            c.host.ambulance_enter();  // state 6, DGROUP:0x914a = 0, seg16:0xcfe(1) [BYTES seg7:0x5fa seg7:0x608]
            return DeadlineResult::kEndPass;
        }
        c.host.seg16_32a0();  // [BYTES seg7:0x60b]
    }
    // DGROUP:0x1e80 [BYTES seg7:0x610-0x705].
    if (due(w.w1e80, now)) {
        if (w.player_game != 0) {  // [BYTES seg7:0x62b seg7:0x630]
            if (w.play_mode > 1) w.play_mode = 1;  // [BYTES seg7:0x6dd seg7:0x6e4]
            if (ship_ok(w.player) && w.ships[static_cast<std::size_t>(w.player)].bec == 0)
                w.w1e84 = now;  // [BYTES seg7:0x6f0 seg7:0x6f7]
            else
                wave_over(c);   // seg9:0x6bc2 [BYTES seg7:0x700]
            w.w1e80 = 0;        // [BYTES seg7:0x6a0]
            return DeadlineResult::kEndPass;  // jmp seg7:0x1192 [BYTES seg7:0x6a9]
        }
        // The demo [BYTES seg7:0x634-0x69a] [OPEN: the demo is not ported].
        if (w.dir().byte(0x1d0) == 2 && w.w0d26 != 0) c.host.demo_seg6_92b8();  // [BYTES seg7:0x64d]
        if (w.w93a4 != 0) {
            // State 8 with DGROUP:0x929c = 0 and the state-8 frame; DGROUP:0x1e80 stays set
            // [BYTES seg7:0x659 seg7:0x667].
            c.host.hall_of_justice();
            return DeadlineResult::kEndPass;
        }
        if (c.setup) wave0_followups(w, *c.setup);  // seg9:0x2828's head [BYTES seg7:0x66a]
        wave_into_directory(c);
        if (ship_ok(w.player) && w.ships[static_cast<std::size_t>(w.player)].bec != 0 && w.w9516 == 0)
            c.host.save_lastgame();  // seg6:0x885c(0, 0) [BYTES seg7:0x687] [DESIGN ADR 0014]
        s.enter_game_over();          // seg9:0xd6c [BYTES seg7:0x68f]
        s.request(shell::kIntro);     // DGROUP:0x13de = 1, DGROUP:0x968e = 0 [BYTES seg7:0x694 seg7:0x69a]
        w.w1e80 = 0;
        return DeadlineResult::kEndPass;
    }
    // DGROUP:0x1e84 [BYTES seg7:0x6ac-0x6da seg7:0x1d8-0x1e6].
    if (due(w.w1e84, now)) {
        // The edition DGROUP:0x920c = 0 (the limited trial) shows the MessageBox first; the tick
        // waits in it, and DGROUP:0x1e50 keeps the value sampled before it [BYTES seg7:0x6bf
        // seg7:0x6c4 seg7:0x6d2].
        if (w.w920c == 0) c.host.history_box();
        w.w1e84 = 0;                     // [BYTES seg7:0x1d8]
        s.deadlines().scores_due = 0;    // the same DGROUP word in the shell's copy [DESIGN]
        // seg6:0x5d58 [BYTES seg7:0x1e1]: state 5 with DGROUP:0x1e7c = DGROUP:0x1e50 + 34000 at
        // its end. That DGROUP:0x1e50 was sampled before the MessageBox, so when the player
        // keeps the box up longer than 34 s the next pass finds DGROUP:0x1e7c passed and goes to
        // state 4 (GAME OVER) at once: the unregistered run of core-0001 waited 5.7 min and saw
        // no table [BYTES seg6:0x5f5e-0x5f6b seg7:0x430 seg7:0x437] [ORACLE core-0001]. The
        // table seg6:0x5f7a / seg6:0x62b6 and the .ded save seg6:0x885c(1, 1) (which the trial
        // refuses silently, formats.md) are the hooks' [OPEN] [DESIGN ADR 0014].
        s.enter_high_scores(false);
        return DeadlineResult::kEndPass;  // [BYTES seg7:0x1e6]
    }
    return DeadlineResult::kContinue;
}

void wave_over(WaveEndCtx& c) {
    World& w = c.w;
    c.host.music(-1, 1);              // seg10:0x7480(-1, 1) [BYTES seg9:0x6bd4]
    c.s.deadlines().debrief_nav = 0;  // DGROUP:0x1e74 [BYTES seg9:0x6bdf]
    if (w.kind == 6) c.host.race_credit();  // [BYTES seg9:0x6be3] [OPEN M6]
    if (w.w93a4 != 0) {
        c.host.hall_of_justice();  // [BYTES seg9:0x6c5b seg9:0x6c62] [OPEN M6]
        return;
    }
    debrief_entry(c, 0);  // [BYTES seg9:0x6cb8]
}

void debrief_entry(WaveEndCtx& c, int arg) {
    World& w = c.w;
    c.host.reset_sounds();         // seg10:0x7db8 [BYTES seg9:0x6cd1]
    c.host.close_watch_windows();  // seg13:0x65f2(1) [BYTES seg9:0x6cd8]
    // DGROUP:0x1e74 = 0, state 2, DGROUP:0x92e4 = 0 [BYTES seg9:0x6ce1 seg9:0x6ce5 seg9:0x6ceb].
    c.s.enter_debriefing(false);
    w.restored = 0;
    c.host.clear_restored();
    if (w.kind == 6) c.host.race_refuel();  // seg9:0x6db0 [BYTES seg9:0x6cf6] [OPEN M6]
    // DGROUP:0x92e0: the port allocates no such block [BYTES seg9:0x6cf9-0x6d1b] [DESIGN].
    c.host.delete_wave_objects();  // seg9:0x1092 [BYTES seg9:0x6d21]
    for (int p = 0; p <= 14; ++p)  // [BYTES seg9:0x6d30-0x6d53]
        for (int r = 0; r < w.pools.allocated(p) && r < w.pools.records(p); ++r) c.host.free_buffer(p, r);
    c.host.close_hud();         // seg11:0x2f0 [BYTES seg9:0x6d5c]
    c.host.release_wave_dcs();  // seg7:0x49b6 [BYTES seg9:0x6d61]
    c.host.black_client();      // [BYTES seg9:0x6d7a]
    w.w93b2 = w.w9332;          // [BYTES seg9:0x6d82]
    if (c.setup) wave0_followups(w, *c.setup);  // seg9:0x2828's head [BYTES seg9:0x6d85]
    wave_into_directory(c);
    c.host.free_blocks();       // seg6:0x7266 [BYTES seg9:0x6d8a]
    w.w94c4 = 1;                // [BYTES seg9:0x6d8f]
    w.w9330 = 0;                // [BYTES seg9:0x6d95]
    c.host.debriefing_started();
    // arg != 0: seg9:0x6dec(1) [BYTES seg9:0x6d9b seg9:0x6da4]; the host's started hook
    // draws the first frame when it is asked to (the state-8 caller, M6) [OPEN M6].
    (void)arg;
}

void pack_remaining_icons(World& w) {
    DirRecord& d = w.dir();
    const Pools& p = w.pools;
    auto in_play = [&](int k) { return k < p.records(0) && p.rec(0, k).state == 0; };
    int s = 0;  // [bp-4]
    int j = 0;  // [bp-0x1c]
    while (j < 8) {  // [BYTES seg9:0x301f]
        int k = 0;   // [bp-0xc]
        if (!in_play(j)) {  // [BYTES seg9:0x3032 seg9:0x303b]
            for (k = j + 1; k < 8; ++k)
                if (in_play(k)) {
                    j = k;  // [BYTES seg9:0x2fa4]
                    break;
                }
        }
        if (k >= 8) break;  // [BYTES seg9:0x2fab]
        const ObjectRecord& o = p.rec(0, j);
        if (static_cast<std::int8_t>(o.flame) < 0) d.set_byte(0x1c8 + static_cast<std::size_t>(s), 2);  // [BYTES seg9:0x2fcb]
        if (o.b53 == 2) d.set_byte(0x1c8 + static_cast<std::size_t>(s), 1);  // [BYTES seg9:0x2ff1]
        d.set_word(0x1d2 + 2 * static_cast<std::size_t>(s), o.damage);      // [BYTES seg9:0x3014]
        ++j;
        ++s;
    }
    for (; s < 8; ++s) {  // [BYTES seg9:0x305f seg9:0x3077]
        d.set_word(0x1d2 + 2 * static_cast<std::size_t>(s), 0);
        d.set_byte(0x1c8 + static_cast<std::size_t>(s), 0);
    }
}

void wave_into_directory(WaveEndCtx& c) {
    World& w = c.w;
    if (!(w.wave > 0)) return;  // the wave-0 call is the load's (setup.cpp) [BYTES seg9:0x2aee]
    // The race tables (kind 6 with DGROUP:0x91f8 != 0 and QuickIntro or the demo) [OPEN M6]
    // [BYTES seg9:0x2828].
    c.host.meal_check();  // seg10:0x9672, seg10:0x552(4, 1), seg10:0x565a [BYTES seg9:0x2af0-0x2b0f]
    // Per ship [BYTES seg9:0x2e64-0x2e78 seg9:0x2b88-0x2e54].
    for (int i = 0; i <= w.last_ship && ship_ok(i); ++i) {
        c.host.flame_off(i);  // seg11:0xb09c(0, i) [BYTES seg9:0x2e69]
        ShipRecord& sh = w.ships[static_cast<std::size_t>(i)];
        const int ri = sh.roster_index;
        if (ri == -1) continue;  // [BYTES seg9:0x2e73 seg9:0x2e78]
        // The credit dword +0xdc, capped at 0x4fff6 (signed) and stored back; roster +0x02 =
        // credit / 10 (seg5:0x110) and +0x04 = `resources` (+0xce): the carry-over (core.md
        // "Carry-over") [BYTES seg9:0x2cdb seg9:0x2d09 seg9:0x2d15].
        std::int32_t credit = static_cast<std::int32_t>(static_cast<std::uint16_t>(sh.wdc) |
                                                        (static_cast<std::uint32_t>(static_cast<std::uint16_t>(sh.wde)) << 16));
        if (credit > 0x4fff6) credit = 0x4fff6;
        sh.wdc = static_cast<std::int16_t>(static_cast<std::uint32_t>(credit) & 0xffff);
        sh.wde = static_cast<std::int16_t>(static_cast<std::uint32_t>(credit) >> 16);
        if (ri >= 0 && ri < static_cast<int>(w.roster.size())) {
            RosterRecord& r = w.roster[static_cast<std::size_t>(ri)];
            r.credit10 = static_cast<std::int16_t>(div32(credit, 10));
            r.resources = word_at(sh, 0xce);
        }
        // The other roster writes (+0x3c + k, +0x38 ... +0x3b, +0x28/+0x1e, +0x1d, seg14:0x421c,
        // +0x06, +0x1c, +0x74, +0x75, +0x72, +0x73, +0x76) and the directory's roster bytes
        // +0x1c0: the port's RosterRecord does not hold them [OPEN] [BYTES seg9:0x2b88-0x2e54].
    }
    if (ship_ok(w.player)) w.player_roster = w.ships[static_cast<std::size_t>(w.player)].roster_index;  // [BYTES seg9:0x2e8c]
    if (w.frames > 500u) c.host.roster_ratings();  // [BYTES seg9:0x2e97] [OPEN]
    pack_remaining_icons(w);
    // The counts [BYTES seg9:0x3089-0x31ba].
    DirRecord& d = w.dir();
    d.set_byte(0xa3, 0);
    d.set_byte(0xa0, 0);
    d.set_byte(0xa1, 0);
    for (int k = 0; k < w.pools.in_use(9) && k < w.pools.records(9); ++k)  // DGROUP:0x9418
        if (w.pools.rec(9, k).state == 0) d.set_byte(0xa3, d.byte(0xa3) + 1);
    for (int k = 0; k < w.pools.in_use(3) && k < w.pools.records(3); ++k) {  // DGROUP:0x940c
        const ObjectRecord& o = w.pools.rec(3, k);
        if (o.mass == 0x15e && o.state == 0) d.set_byte(0xa0, d.byte(0xa0) + 1);
        if (o.mass == 0x578 && o.state == 0) d.set_byte(0xa1, d.byte(0xa1) + 1);
    }
    for (int k = 0; k < w.pools.in_use(8) && k < w.pools.records(8) && k < kPool8Max; ++k) {  // DGROUP:0x9416
        const auto kk = static_cast<std::size_t>(k);
        std::int16_t v = w.pools.rec(8, k).damage;  // [BYTES seg9:0x316b]
        if (w.pool8[kk].at(0x0a) == -3) v = sub16(0, v);  // [BYTES seg9:0x3174 seg9:0x319c]
        if (0xae + 2 * kk + 1 < kDirRecordSize) d.set_word(0xae + 2 * kk, v);
        if (0xfe + 2 * kk + 1 < kDirRecordSize) d.set_word(0xfe + 2 * kk, w.pool8[kk].at(0x2e));  // DGROUP:0x3d7e [BYTES seg9:0x31ba]
    }
    // LASTGAME [BYTES seg9:0x31ca-0x31f0] (stage 3, ADR 0014).
    if (ship_ok(w.player) && w.ships[static_cast<std::size_t>(w.player)].bec != 0 && w.wave > 0 &&
        (w.player_game != 0 || w.w9516 == 0))
        c.host.save_lastgame();
}

}  // namespace is::world
