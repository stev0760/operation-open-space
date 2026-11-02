// seg12:0x2288 and the vanish's end (gate_exit.h). 16-bit arithmetic as the original's (ADR 0013
// decision 7).
#include "world/gate_exit.h"

namespace is::world {

namespace {

bool rec_ok(const Pools& p, int pool, int rec) {
    return pool >= 0 && pool < kPoolCount && rec >= 0 && rec < p.records(pool);
}
bool ship_ok(int i) { return i >= 0 && i < kShipCount; }

std::int16_t team_of(GateExitCtx& c, int ship) {
    if (!ship_ok(ship)) return 0;
    return c.host.design_type(c.w.ships[static_cast<std::size_t>(ship)].design_index);
}

}  // namespace

void gate_exit(GateExitCtx& c, int pool, int rec) {
    World& w = c.w;
    Pools& pools = w.pools;
    if (!rec_ok(pools, pool, rec)) return;
    const int ship_pool = w.roles.ships;  // DGROUP:0x9324
    const int player = w.player;          // DGROUP:0x9646
    // 1. Pool DGROUP:0x9328 [BYTES seg12:0x2297 seg12:0x22ad seg12:0x22b2 seg12:0x22c3
    // seg12:0x22cf seg12:0x22d6 seg12:0x22de].
    if (pool == w.roles.pool5) {
        ObjectRecord& o = pools.rec(pool, rec);
        if (o.b2e != 0) return;
        c.host.sound(0x27, pool, rec, 1);
        c.host.pool5_slot(rec);
        o.b2e = 0xff;
        return;
    }
    // 2. A dead ship [BYTES seg12:0x22e6 seg12:0x22f3 seg12:0x22ff].
    if (pool == ship_pool && ship_ok(rec) && w.ships[static_cast<std::size_t>(rec)].bec == 0) {
        c.host.destroy(pool, rec);
        return;
    }
    // 3. The pair note, literal pool 14 [BYTES seg12:0x2308-0x236a].
    if (pool == 14 && ship_ok(rec)) {
        const ShipRecord& s = w.ships[static_cast<std::size_t>(rec)];
        if (s.w52 >= 2 && s.w52 <= 4 && s.w58 == rec && s.w54 == 14) {
            const ShipRecord* src = nullptr;
            if (rec == player) {
                // The state of ship +0x56's object in pool 14 (DGROUP:0x1e30) [BYTES seg12:0x2344].
                if (ship_ok(s.w56) && rec_ok(pools, 14, s.w56) && pools.rec(14, s.w56).state == 0)
                    src = &w.ships[static_cast<std::size_t>(s.w56)];
            } else if (s.w56 == player) {
                src = &s;
            }
            if (src && src->roster_index != -1) w.w93b6 = src->roster_index;  // [BYTES seg12:0x235f seg12:0x236a]
        }
    }
    // 4. The private buffer [BYTES seg12:0x237b seg12:0x2389].
    if (pools.rec(pool, rec).on_screen != 0 && c.entry) entry_buffer(*c.entry, pool, rec);
    // 5. Ship word +0x2e = 1 [BYTES seg12:0x239c].
    if (pool == ship_pool && ship_ok(rec)) w.ships[static_cast<std::size_t>(rec)].w2e = 1;
    // 6. The vanish [BYTES seg12:0x23b0 seg12:0x23bf seg12:0x23d7 seg12:0x23e3 seg12:0x23f5].
    if (pools.rec(pool, rec).on_screen != 0) {
        if (c.entry) entry_start(*c.entry, 1, pool, rec);
    } else {
        pools.rec(pool, rec).state = 1;
        c.host.removed(pool, rec);
        c.host.sound(0x27, pool, rec, 1);
    }
    // 7. A team-7 ship taking its prisoner through [BYTES seg12:0x23fd-0x2472].
    if (pool == ship_pool && ship_ok(rec)) {
        const ShipRecord& s = w.ships[static_cast<std::size_t>(rec)];
        if (team_of(c, rec) == 7 && s.w54 == ship_pool) {
            ObjectRecord& o = pools.rec(pool, rec);
            if (o.w36 == ship_pool && o.w38 == s.w56) {
                gate_exit(c, ship_pool, o.w38);  // [BYTES seg12:0x2444]
                if (pools.rec(ship_pool, rec).w38 == player) {  // [BYTES seg12:0x2458]
                    w.w1e80 = c.now + 1000u;                      // [BYTES seg12:0x246b]
                    w.w93a4 = 1;                                  // [BYTES seg12:0x2472]
                }
            }
        }
    }
    // 8. The player held by a team-7 ship [BYTES seg12:0x2478-0x24b1].
    if (pool == ship_pool && rec == player) {
        const ObjectRecord& o = pools.rec(pool, rec);
        if (o.owner_pool == ship_pool && team_of(c, o.owner_rec) == 7) w.w93a4 = 1;
    }
    // 9. Demon's Gate, ship 1: the player's object byte +0x54 = 0 [BYTES seg12:0x24b7-0x24d2].
    if (pool == ship_pool && w.handover.demons_gate != 0 && rec == 1 && rec_ok(pools, 14, player))
        pools.rec(14, player).b54 = 0;
    // 10. An icon: seg7:0x5ae8(rec, 0) [BYTES seg12:0x24d7 seg12:0x24e2].
    if (pool == 0) c.host.icon_counted(rec, 0);
    // 11. A duel: DGROUP:0x668a = -1 when no other live ship is in play [BYTES seg12:0x24ea-0x2546].
    if (w.kind == 7 && pool == ship_pool) {
        bool none = true;
        for (int k = 0; k <= w.last_ship && ship_ok(k); ++k) {
            if (k == rec || k == player) continue;
            if (w.ships[static_cast<std::size_t>(k)].bec == 0) continue;
            if (rec_ok(pools, 14, k) && pools.rec(14, k).state == 0) none = false;
        }
        if (none) w.gate[0].w12 = -1;
    }
}

int award_entry(const World& w, int pool, int rec) {
    // seg5:0x2b4(dword DGROUP:0x1e68, 10): the unsigned long division [BYTES seg7:0x190a].
    const std::uint32_t t = w.frames / 10u;
    for (int k = 0; k < 8; ++k) {
        // dx:ax - cwd(word): the high word must be 0 and the low word below 20 [BYTES
        // seg7:0x191d seg7:0x1921 seg7:0x1925].
        const std::int32_t word = w.w937a[static_cast<std::size_t>(k)];
        const std::uint32_t d = t - static_cast<std::uint32_t>(word);
        if ((d >> 16) != 0 || (d & 0xffffu) >= 20u) return -1;  // ends the search [BYTES seg7:0x1923 seg7:0x1928]
        const int hp = static_cast<std::int8_t>(w.hit_pool[static_cast<std::size_t>(k)]);  // cbw
        const int hr = static_cast<std::int8_t>(w.hit_rec[static_cast<std::size_t>(k)]);
        if (hp == pool && hr == rec) return k;  // [BYTES seg7:0x1936 seg7:0x1941]
    }
    return -1;
}

void push_hit(World& w, int pool, int rec) {
    for (int k = 7; k > 0; --k) {  // [BYTES seg8:0x820f]
        w.w937a[static_cast<std::size_t>(k)] = w.w937a[static_cast<std::size_t>(k - 1)];
        w.hit_pool[static_cast<std::size_t>(k)] = w.hit_pool[static_cast<std::size_t>(k - 1)];
        w.hit_rec[static_cast<std::size_t>(k)] = w.hit_rec[static_cast<std::size_t>(k - 1)];
    }
    w.w937a[0] = static_cast<std::int16_t>(w.frames / 10u);  // the low word [BYTES seg8:0x8240]
    w.hit_pool[0] = static_cast<std::uint8_t>(pool);         // [BYTES seg8:0x8246]
    w.hit_rec[0] = static_cast<std::uint8_t>(rec);           // [BYTES seg8:0x824c]
}

void vanish_end(GateExitCtx& c, int pool, int rec) {
    World& w = c.w;
    Pools& pools = w.pools;
    if (!rec_ok(pools, pool, rec)) return;
    ObjectRecord& o = pools.rec(pool, rec);
    // A ship whose word +0x2e = 2: byte +0x2e = 0xfe, and with PlayMode != 0, when it is the
    // player's goal (+0x32/+0x34) and the player's word +0x38 = 1, the message 1422 in 1200 ms;
    // no buffer free (correction 13) [BYTES seg7:0x181e-0x1898].
    if (pool == 14 && ship_ok(rec) && w.ships[static_cast<std::size_t>(rec)].w2e == 2) {
        o.b2e = 0xfe;  // [BYTES seg7:0x1842]
        if (w.play_mode != 0 && ship_ok(w.player)) {  // [BYTES seg7:0x1847]
            const ShipRecord& p = w.ships[static_cast<std::size_t>(w.player)];
            if (p.w32 == pool && p.w34 == rec && p.w38 == 1) {  // [BYTES seg7:0x185a seg7:0x1864 seg7:0x186a]
                w.w05c0 = c.now + 1200u;  // [BYTES seg7:0x187e]
                w.w058c = 1;              // [BYTES seg7:0x1885]
                w.w8630 = c.host.lang(1422);  // seg10:0xa332(DGROUP:0x8630, 0x58e, 1) [BYTES seg7:0x1890]
            }
        }
        return;  // the tail seg7:0x1acb (end_slot)
    }
    o.state = 1;                 // [BYTES seg7:0x18b7]
    c.host.column_row(pool, rec);  // seg7:0x65aa [BYTES seg7:0x18c5]
    // The 800 award for pools other than 14, 6, 7 and 2 (correction 15: one search, at most one
    // award) [BYTES seg7:0x18cd-0x196a].
    if (pool != 14 && pool != 6 && pool != 7 && pool != 2) {
        if (award_entry(w, pool, rec) >= 0) {
            c.host.sound(0x73, pool, rec, 1);  // [BYTES seg7:0x1950]
            c.host.message(w.player, 0x70);    // seg3:0x6ec(player, 7, 0, 0x70) [BYTES seg7:0x1962]
            w.score += 800u;                   // [BYTES seg7:0x196a]
        }
    }
    if (pool == 6) {
        // The weapon-slot reset [BYTES seg7:0x1983 seg7:0x1988 seg7:0x198c seg7:0x1990 seg7:0x19ae].
        MissileSlot& e = w.pools.missile(rec);  // DGROUP:0x66e0 + 0x1c·rec (records.h)
        e.age = c.host.weapon_word_1e(e.weapon);
        e.weapon = -1;
        o.b2e = 0;
    } else {
        o.b2e = 0xff;     // [BYTES seg7:0x19d1]
        c.host.lines();   // seg12:0x3778 [BYTES seg7:0x19d6]
    }
    if (c.entry) entry_buffer_free(*c.entry, pool, rec);  // seg12:0x26a4 [BYTES seg7:0x19e6]
    c.host.removed(pool, rec);  // seg7:0x2080 [BYTES seg7:0x19f4]
    c.host.menu_refresh();      // seg2:0x170e [BYTES seg7:0x19fa]
}

int vanish_frames(std::int16_t size, std::int16_t size_level) {
    const int n = entry_blocks(size, size_level);
    const int total = n * n;
    return (total + 9) / 10;
}

}  // namespace is::world
