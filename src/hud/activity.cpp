// The activity text and the object names (activity.h).
#include "hud/activity.h"

#include <map>

#include "hud/messages.h"
#include "hud/target.h"
#include "world/pilot.h"
#include "world/relations.h"
#include "world/world.h"

namespace is::hud {

using world::ship_b;
using world::ship_d;
using world::ship_sb;
using world::ship_w;

namespace {

using FArgs = std::vector<briefing::FArg>;

bool is_lower_case_letter(char c) {
    const auto v = static_cast<signed char>(c);
    return v < 0x7b && v > 0x60;
}

// One log per site (world.h guard_todo) [DESIGN ADR 0016 decision 7].
void log_site(const char* what, const char* address) {
    static std::map<std::string, bool> once;
    world::guard_todo(once[std::string(address) + what], what, address);
}

// The object record word +off; a record the port does not hold is the zeroed scratch record,
// so every word and byte of it reads 0 and `state` 0 is in play (review L4)
// [DESIGN ADR 0016 decision 2].
int obj_word(const world::World& w, int pool, int rec, int off) {
    if (pool < 0 || pool >= world::kPoolCount || rec < 0 || rec >= w.pools.records(pool)) {
        log_site("seg3: a pool record outside the port's pools reads 0", "seg3:0x1d0a");
        return 0;
    }
    return world::word_at(w.pools.rec(pool, rec), off);
}
int obj_byte(const world::World& w, int pool, int rec, int off) {
    if (pool < 0 || pool >= world::kPoolCount || rec < 0 || rec >= w.pools.records(pool)) {
        log_site("seg3: a pool record outside the port's pools reads 0", "seg3:0x1d0a");
        return 0;
    }
    return reinterpret_cast<const unsigned char*>(&w.pools.rec(pool, rec))[off];
}
// Object `state` +0x18.
int obj_state(const world::World& w, int pool, int rec) { return obj_word(w, pool, rec, 0x18); }

// The pool-8 side table word DGROUP:0x3d50 + 0x34·rec + off (world.h Pool8Entry); past the table
// 0 [DESIGN ADR 0016 decision 2].
int pool8_word(const world::World& w, int rec, int off) {
    if (rec < 0 || rec >= world::kPool8Max) {
        log_site("seg3: a pool-8 side-table entry past the table reads 0", "seg3:0x181d");
        return 0;
    }
    return w.pool8[static_cast<std::size_t>(rec)].at(off);
}

// The design word +0x1e (DGROUP:0x96f0 + 0xad·n), the tank (rating.h DesignRating::tank); a
// design the port does not hold reads 0 [DESIGN ADR 0016 decision 2].
int design_tank(const world::World& w, int n) {
    if (n >= 0 && n < world::kShipCount) return w.design_ratings[static_cast<std::size_t>(n)].tank;
    log_site("seg3: a design word +0x1e outside the designs reads 0", "seg3:0x2f0f");
    return 0;
}

// A ship's design n (ship word +0x4e); a ship the port does not hold gives -1, which names no
// design [DESIGN ADR 0016 decision 2].
int design_of(const world::World& w, int ship) {
    if (ship < 0 || ship >= world::kShipCount) {
        log_site("seg3: a ship record outside the eight records names no design", "seg3:0x1588");
        return -1;
    }
    return w.ships[static_cast<std::size_t>(ship)].design_index;
}

}  // namespace

// seg11:0x3fd4 is briefing::case_3fd4 (lang_format.h), shared with the Hall of Justice.

int name_gender(std::string& buf) {
    // [BYTES seg3:0x1af4-0x1b7f]. An empty name reads the byte before the buffer in the
    // original; the port reads it as NUL, which names no gender, so 6 [DESIGN ADR 0016 decision 2].
    int g = -1;
    if (buf.empty()) log_site("seg3:0x1ae6 on an empty name: the byte before the buffer reads NUL", "seg3:0x1af4");
    if (!buf.empty()) {
        const char c = buf.back();
        if (c == 'M') g = 0;
        else if (c == 'F') g = 2;
        else if (c == 'N') g = 6;
        else if (c == 'P') g = 4;
    }
    if (g == -1) return 6;  // seg3:0x1b7a
    if (buf.size() >= 2) buf.resize(buf.size() - 2);  // seg3:0x1b73: buf[len - 2] = 0
    return g;
}

std::string pilot_name_8e60(const world::World& w, ActivityHost& h, int i) {
    if (i < 0 || i >= world::kShipCount) return {};
    const world::ShipRecord& S = w.ships[static_cast<std::size_t>(i)];
    if (S.roster_index >= 0) {  // [BYTES seg17:0x8e7c-0x8eb3]
        if (static_cast<std::size_t>(S.roster_index) >= w.roster.size()) {  // [DESIGN ADR 0016 decision 2]
            log_site("seg17:0x8e60 for a roster record the port does not hold: no name", "seg17:0x8e7c");
            return {};
        }
        std::string s;
        for (char c : w.roster[static_cast<std::size_t>(S.roster_index)].name) {
            if (c == 0) break;
            s += c;
        }
        return s;
    }
    using world::add16;
    using world::mul16;
    const std::int16_t c = ship_sb(S, world::pw::kClass);
    const std::int16_t ii = static_cast<std::int16_t>(i);
    std::int16_t ax = add16(add16(ii, ii), 3);
    ax = mul16(ax, add16(c, 2));
    ax = mul16(ax, add16(ii, 6));
    ax = mul16(ax, add16(ii, 7));
    ax = add16(ax, ax);
    const std::int16_t d = add16(c, 3);
    std::int16_t q = 0;
    if (d != 0)
        q = world::idiv16(ax, d);
    else
        h.todo("seg17:0x8e60's divisor c + 3 = 0 gives 0", "seg17:0x8ee3");
    std::string officer;
    h.load(788, officer, 0x28);
    return officer + " " + std::to_string(add16(q, 0x3bc));
}

std::string object_name(const world::World& w, ActivityHost& h, int caps, int pool, int rec, int mode,
                        std::string buf) {
    int g = -1;      // [bp-0x38] [BYTES seg3:0x1550]
    int plural = 0;  // [bp-0x3c], the dword store that also zeroes the vowel flag [bp-0x3a]
    int v = 0;       // [BYTES seg3:0x1555-0x1558]
    const auto load = [&](int id) { h.load(id, buf, 0x28); };
    const auto one_five_to_two = [&] {  // seg3:0x171d-0x172b
        if (mode == 1 || mode == 5) mode = 2;
    };
    if (mode == 7) {  // seg3:0x155c-0x156c: the plural
        plural = 10000;
        mode = 5;
        g = 4;
    }
    if (mode == 5 && h.w967a() != 0) mode = 1;  // seg3:0x1571-0x157d
    if (pool == world::kPoolShips) {            // the literal 14 [BYTES seg3:0x1582]
        // lstrcpy(buf, design +0x70 of ship rec's design) [BYTES seg3:0x1588-0x159c]; a design the
        // port does not hold names nothing [DESIGN ADR 0016 decision 2] (logged by design_of).
        const int n = design_of(w, rec);
        buf = n >= 0 && n < world::kShipCount ? w.design_names[static_cast<std::size_t>(n)] : std::string();
        if (mode == 5) mode = 1;                                  // seg3:0x15a1-0x15a7
        g = (h.design_flags44(n) & 0x20) != 0 ? 0 : 2;            // seg3:0x15b7-0x15c6
    } else if (pool == 0) {                                       // seg3:0x15ce
        if (mode == 2) mode = 5;                                  // seg3:0x15d5-0x15db
        load(0x98);                                               // 152 "Icon ^N"
        if (!buf.empty()) {
            const char c = buf.back();                            // seg3:0x1600-0x163b
            if (c == 'M') g = 0;
            else if (c == 'F') g = 2;
            else if (c == 'N') g = 6;
        }
        // seg3:0x164c: buf[len - 2] = 0; for a shorter text the store lands before the buffer and is
        // lost [DESIGN ADR 0016 decision 2].
        if (buf.size() >= 2) buf.resize(buf.size() - 2);
        else log_site("seg3:0x1542's icon name shorter than two characters: the cut is lost", "seg3:0x164c");
        buf += std::to_string(static_cast<std::int16_t>(rec));  // seg6:0xf1a [BYTES seg3:0x1659]
        if (caps != 0) {  // seg3:0x166f-0x169e: " (", the file name, ")"
            buf += " (";
            if (rec >= 0 && rec < world::kIconRecords) buf += w.icons[static_cast<std::size_t>(rec)].name;
            buf += ")";
        }
    } else if (pool == 1) {
        load(0x37c);  // 892 [BYTES seg3:0x16ab]
    } else if (pool == 7) {
        load(0x15b);  // 347 "Ambulance^N" [BYTES seg3:0x16c6]
    } else if (pool == 2) {
        one_five_to_two();  // seg3:0x16d1-0x16dd
        load(0x9a);         // 154 "Defender^N"
    } else if (pool == 9 || (pool == 3 && target_kind_371c(pool, rec) == 0)) {
        load((pool == 9 ? 0xaa : 0x99) + plural);  // 170 / 153 (+10000) [BYTES seg3:0x16f5 seg3:0x1772]
        if (plural == 0) {                         // seg3:0x1707-0x171a
            g = name_gender(buf);
            one_five_to_two();
        }
    } else if (target_kind_371c(pool, rec) != 0) {
        load(0x249);              // 585 "Exit Gate^N" [BYTES seg3:0x1733-0x1750]
        if (mode == 5) mode = 1;  // seg3:0x1755-0x175d
    } else if (pool == 4) {
        // By DGROUP:0x9214, then the record's word +0x26 [BYTES seg3:0x177f-0x1815].
        const int a26 = obj_word(w, 4, rec, 0x26);
        switch (w.w9214) {
        case 0: load(0x9b); break;
        case 1: load(0xeb); break;
        case 2: load(0xec); break;
        case 3: load(a26 == 0 ? 0xed : 0xee); break;
        case 4: load(a26 == 0 ? 0xf1 : 0xf2); break;
        default: break;
        }
        g = name_gender(buf);  // seg3:0x17a6-0x17b1
    } else if (pool == 8) {
        if (pool8_word(w, rec, 0x1c) == 2) {
            // The hazard class's name word DGROUP:0x4da2 + 0x14·(DGROUP:0x3d80 + 0x34·rec), +plural
            // [BYTES seg3:0x181d-0x183f]: World::classes byte +0x12 of the class record.
            const int k = pool8_word(w, rec, 0x30);
            int id = 0;
            if (k >= 0 && k < static_cast<int>(w.classes.size()))
                id = static_cast<std::int16_t>(w.classes[static_cast<std::size_t>(k)][0x12] |
                                               (w.classes[static_cast<std::size_t>(k)][0x13] << 8));
            load(id + plural);
            if (plural == 0) one_five_to_two();  // seg3:0x1844-0x184c
        } else {
            if (plural == 0) one_five_to_two();  // seg3:0x1855-0x1867
            const int kind = pool8_word(w, rec, 0x1c);
            if (kind == 3) load(0x16a + plural);       // 362 "Gate Block" [BYTES seg3:0x1870-0x187e]
            else if (kind == 1) load(0x16b + plural);  // 363 "Hazard Block" [BYTES seg3:0x1889-0x1899]
        }
    } else if (pool == 5) {
        one_five_to_two();  // seg3:0x18a4-0x18b0
        const int a44 = obj_word(w, 5, rec, 0x44);
        if (a44 == 0x78) load(0x1ec);        // 492 "Fireball^N" [BYTES seg3:0x18c3-0x18ce]
        else if (a44 == 0xc8) load(0x1ed);   // 493 "Snowball^N" [BYTES seg3:0x18e2-0x18f0]
    } else if (pool == 6 && h.missile_weapon(rec) > 0 && h.weapon_name(h.missile_weapon(rec)) != -1) {
        load(h.weapon_name(h.missile_weapon(rec)) + 10000);  // [BYTES seg3:0x18fc-0x1925]
        one_five_to_two();                                    // seg3:0x192a -> 0x171d
    } else if (pool == 0xb) {
        load(0x39e);  // 926 "Resource Pack^N" [BYTES seg3:0x1937]
    } else {
        load(0xa9);  // 169 "Object #" and the pool [BYTES seg3:0x193d-0x1968]
        buf += std::to_string(static_cast<std::int16_t>(pool));
    }
    // ---- the article [BYTES seg3:0x196d-0x1add]
    if (!buf.empty()) {
        const char c = buf[0];
        if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'a' || c == 'e' || c == 'i' ||
            c == 'o' || c == 'u')
            v = 1;  // seg3:0x1970-0x199f
    }
    if (g == -1) g = name_gender(buf);  // seg3:0x19a4-0x19b5
    std::string article;                // [bp-0x34]
    const int base = mode == 1 ? 0xa8c : mode == 2 ? 0xaa0 : mode == 6 ? 0xac8 : mode == 3 ? 0xab4 : 0;
    if (base != 0) h.load(base + v + g, article, 0x28);  // 2700 / 2720 / 2760 / 2740 + v + g
    if (caps != 0) {  // seg3:0x1a1d-0x1a75: one trailing space cut, a ... z upper-cased
        if (!buf.empty() && buf.back() == ' ') buf.pop_back();
        for (char& ch : buf)
            if (is_lower_case_letter(ch)) ch = static_cast<char>(ch - 0x20);
    }
    if (mode != 0 && !article.empty()) buf = article + buf;  // seg3:0x1a77-0x1adb
    return buf;
}

void lines_31d0(ActivityState& st, int mode, std::string l1, std::string l2) {
    if (!l2.empty() && l2[0] == '.') {  // seg3:0x31de-0x3229: the dot moves to line 1
        l1 += ".";
        l2.erase(0, 1);
    }
    if (l2.empty()) {  // seg3:0x322e-0x3265 (an empty line 1 reads the byte before it [DESIGN])
        const char last = l1.empty() ? '\0' : l1.back();
        if (last != '.' && last != '!') l1 += ".";
    }
    switch (mode) {  // seg3:0x3313-0x3322
    case 0:
    case 1:  // seg3:0x32e4-0x330c
        st.w8630 = l1;
        if (mode == 1) st.w8630 += " " + l2;
        break;
    case 2:  // seg3:0x3270-0x32a7
        st.w8db0 = l1 + " " + l2;
        split_lines(st.w8db0, st.w8768, st.w87b8);
        break;
    case 3:  // seg3:0x32ac-0x32e2
        st.w8694 = case_3fd4(l1, 1);
        st.w86d0 = case_3fd4(l2, 1);
        break;
    default: break;
    }
}

void activity_text(world::World& w, ActivityHost& h, ActivityState& st, int a, int s, int mode) {
    if (s < 0 || s >= world::kShipCount) {  // a ship the port does not hold [DESIGN ADR 0016 decision 2]
        h.todo("seg3:0x1d0a for a ship index outside the records", "seg3:0x1d19");
        return;
    }
    world::ShipRecord& S = w.ships[static_cast<std::size_t>(s)];
    const int P = w.player;
    const bool pg = w.player_game != 0;  // DGROUP:0x95bc
    const int pm = w.play_mode;          // DGROUP:0x9134
    const std::uint32_t now = h.now();   // DGROUP:0x1e50
    const int gp = ship_w(S, world::pw::kGoalPool), gr = ship_w(S, world::pw::kGoalRec);  // [bp-4], [bp-6]
    const auto load = [&](int id, std::string& buf) { h.load(id, buf, 0x28); };
    const auto lang = [&h](int id) { return h.lang(id); };
    // seg10:0xa332(buf, id, 1, ...): overwrite.
    const auto fmt = [&](int id, const FArgs& args) { return briefing::lang_format(h.lang(id), args, lang); };
    // %D's (pool, rec, code): seg3:0x1542(code in {0, 2, 4, 7}, pool, rec, buf, code) [BYTES
    // seg10:0xa754 seg10:0xa787 seg10:0xa7ba], as three arguments of the port's formatter.
    const auto D = [&](int pool, int rec, int code) -> std::vector<briefing::FArg> {
        const int up = code == 0 || code == 2 || code == 4 || code == 7 ? 1 : 0;
        return {briefing::FArg(object_name(w, h, up, pool, rec, code)), briefing::FArg(0), briefing::FArg(0)};
    };
    const auto cat = [](std::vector<briefing::FArg> x, const std::vector<briefing::FArg>& y) {
        x.insert(x.end(), y.begin(), y.end());
        return x;
    };
    std::string N;   // [bp-0x34]: uninitialised in the original; empty [DESIGN ADR 0016 decision 1]
    std::string L0;  // [bp-0x5c]
    std::string V;   // [bp-0x98]
    std::string R;   // [bp-0x110]: uninitialised in the original; empty [DESIGN ADR 0016 decision 1]
    int e = -1;      // [bp-0xc]
    const int order = ship_w(S, world::pw::kOrder);
    const int b146 = ship_b(S, world::pw::kReaction);

    // 1. The name and the prefix [BYTES seg3:0x1d19-0x1dd5].
    if (gp == 14 && gr == a && (s != P || pm == 0 || !pg))
        load(0x1c8, N);  // 456 "You."
    else
        N = object_name(w, h, 1, gp, gr, 5, N);
    load(s == P && pg && h.w9460() == 0 ? 0x43a : 0x43b, L0);  // 1082 "You Are " / 1083 "I'm "
    if (gp == 3) N = object_name(w, h, 1, 3, gr, 0, N);           // seg3:0x1da9-0x1dc2: no article

    // 2. The ambulance's ship [BYTES seg3:0x1dda-0x1e99].
    if (s == w.w952e && w.w9e49 >= 0 && w.w9e49 < 9) {
        int rid;
        const bool amb_in_play = obj_state(w, 7, 0) == 0;  // far DGROUP:0x1e14 record 0 +0x18
        if (w.w9e49 < 6 && (w.w1e88 != 0 || amb_in_play)) {
            if (ship_b(S, 0x147) == 0) {
                const std::int16_t sum = world::add16(world::add16(S.w26, S.w28), S.w2a);
                if (sum < 100) {
                    load(0x1e8, V);                  // 488 "Waiting For Help"
                    rid = a == 100 ? 0x460 : 0x1e9;  // 1120 / 489
                } else {
                    load(0x46c, V);  // 1132
                    rid = 0x46d;     // 1133
                }
            } else {
                load(0x442, V);  // 1090 "Getting Serviced"
                rid = 0x443;     // 1091
            }
        } else {
            load(0x46e, V);  // 1134 "Waiting For The Ambulance"
            rid = 0x46f;     // 1135
        }
        load(rid, R);  // seg3:0x1e95
    } else if (w.kind == 6 && world::ship_team(w, s) != 7) {
        // 3. Race waves (ruling R3): the race block is not ported; the lines stay empty.
        h.todo("the race activity text (1079-1089 and the time gap)", "seg3:0x1eec");
        V.clear();
        R.clear();
    } else {
        // 4. By the goal's activity +0x38 [BYTES seg3:0x20b5-0x20d9].
        const int act = ship_w(S, world::pw::kGoalAct);
        if (act == 1) {
            // ---- attack [BYTES seg3:0x20f9-0x2a6e]
            V = fmt(0x1c9, {briefing::FArg(N)});  // 457 "Attacking %s"
            const std::uint32_t a4 = ship_d(S, world::pw::kGoalSince);
            bool done = false;
            if (!pg && a4 + 14000u < now && now < a4 + 19000u) {  // seg3:0x2111-0x2152
                e = 0x467;                                         // 1127
                done = true;
            }
            if (!done && !pg && a4 + 19000u < now &&  // seg3:0x215a-0x219a
                (order == 0 || (!(ship_d(S, world::pw::kOrderUntil) >= now) && order != 4 && order != 1))) {
                e = 0x468;  // 1128, and the give-up [BYTES seg3:0x219f-0x222b]
                done = true;
                bool retargeted = false;
                if (w.kind == 7) {  // seg3:0x21a4-0x21fa: the duel's next ship
                    for (int k = 0; k <= w.last_ship; ++k) {
                        if (k == s || ship_w(S, world::pw::kOrderRec) == k) continue;
                        if (obj_state(w, world::kPoolShips, k) != 0) continue;  // far DGROUP:0x1e30
                        world::set_ship_w(S, world::pw::kOrderRec, static_cast<std::int16_t>(k));
                        world::set_ship_d(S, world::pw::kGoalSince, now);
                        retargeted = true;
                        break;
                    }
                } else if (gp == 14) {  // seg3:0x222e-0x2257
                    if (gr >= 0 && gr < world::kShipCount) {
                        const int off = world::pw::kAttitude0 + 2 * gr;
                        world::set_ship_w(S, off, static_cast<std::int16_t>(ship_w(S, off) / 2));  // cwd; idiv 2
                        world::set_ship_b(S, world::pw::kGrudge0 + gr, 0xff);
                    } else {  // the word and byte past the record's arrays [DESIGN ADR 0016 decision 2]
                        h.todo("seg3:0x1d0a's give-up for a goal record outside 0 ... 7", "seg3:0x2248");
                    }
                }
                if (!retargeted) {  // seg3:0x21fe-0x2226
                    world::set_ship_d(S, world::pw::kReplanAt, 0);
                    world::set_ship_w(S, world::pw::kOrder, -1);
                    if (s != P || !pg) h.think(s);  // seg4:0x78(s)
                }
            }
            if (!done) {
                if (b146 == 0x0d) {  // seg3:0x225e-0x2280
                    e = gr == a ? 0x1f0 : 0x1f1;  // 496 / 497
                } else if ((order > 1 && order < 5 && (s != P || !pg)) || w.kind == 7) {
                    if (ship_w(S, world::pw::kOrderBy) != s) {
                        // 459 "As Requested By %D." with (14, +0x58, the offset of [bp-0x34]) and a
                        // leftover 5: the code is a stack address, so the name has no article and
                        // no capitals [BYTES seg3:0x26fe-0x271f]; the port passes code -1.
                        R = fmt(0x1cb, D(14, ship_w(S, world::pw::kOrderBy), -1));
                    } else if (mode != 3 || ship_w(S, world::pw::kOrderPool) != 14 ||
                               ship_w(S, world::pw::kOrderRec) != P) {
                        const bool at_a = gp == 14 && gr == a;
                        if (b146 == 0x14) e = at_a ? 0x6c5 : 0x6ab;       // 1733 / 1707
                        else if (b146 == 0x15) e = at_a ? 0x6c6 : 0x6ac;  // 1734 / 1708
                        else e = 0x1ca;                                    // 458
                    } else {
                        // The duel commentary 1710-1747 (ruling R3: duels are later).
                        h.todo("the duel commentary of the watch (1710-1747)", "seg3:0x2390");
                    }
                } else if (order == 1) {  // order 1: "To Help %s."
                    const int k = ship_w(S, world::pw::kOrderRec);
                    if (k == a)
                        load(0x1c8, N);
                    else
                        N = object_name(w, h, 0, 14, k, 5, N);
                    e = 0x28dc;  // 10460
                } else if (b146 == 1) {
                    e = 0x6a6;  // 1702
                } else if (b146 == 2) {
                    int k = -1;  // the helper whose goal is the same and who attacks it
                    for (int j = 0; j <= w.last_ship; ++j) {
                        if (obj_state(w, world::kPoolShips, j) != 0 || j == s || j < 0 || j >= world::kShipCount)
                            continue;
                        const world::ShipRecord& J = w.ships[static_cast<std::size_t>(j)];
                        if (ship_w(J, world::pw::kGoalPool) == gp && ship_w(J, world::pw::kGoalRec) == gr &&
                            ship_w(J, world::pw::kGoalAct) == 1) {
                            k = j;
                            break;
                        }
                    }
                    if (k == -1)
                        e = 0x6a7;  // 1703
                    else            // 1077 [BYTES seg3:0x27fe-0x2841]
                        R = fmt(0x435, cat(D(14, k, 5), {briefing::FArg(gp == 14 ? 1 : 0), briefing::FArg(gr == a ? 1 : 0),
                                                        briefing::FArg(0x1cf), briefing::FArg(0x1c8),
                                                        briefing::FArg(gp != 14 ? 1 : 0)}));
                } else if (gp == 2) {
                    e = 0x1cd;  // 461
                } else if (order == 0x17) {
                    e = 0x1ef;  // 495
                } else if (b146 == 10) {
                    e = 0x6a8;  // 1704
                } else if (b146 == 0x0f) {
                    e = 0x6a9;  // 1705
                } else if (b146 == 0x10) {
                    e = 0x6aa;  // 1706
                } else if (gp != 14) {
                    const int team = world::ship_team(w, s);
                    if (gp == 0) {
                        const int slot = S.weapon_slot;
                        const int wpn = slot >= 0 && slot < 10 ? S.weapon[static_cast<std::size_t>(slot)] : 0;
                        if (obj_byte(w, 0, gr, 0x53) == 2) {
                            e = 0x456;  // 1110
                        } else if (static_cast<signed char>(obj_byte(w, 0, gr, 0x2f)) < 0 && wpn == 5) {
                            e = 0x6ad;  // 1709
                        } else {
                            // 1111 ".I Hate " + the file name up to its '.' + "!" [BYTES
                            // seg3:0x2876-0x28ea]
                            load(0x457, R);
                            std::string nm = gr >= 0 && gr < world::kIconRecords
                                                 ? w.icons[static_cast<std::size_t>(gr)].name
                                                 : std::string();
                            const auto dot = nm.find('.');
                            if (dot != std::string::npos) nm.resize(dot);
                            R += nm + "!";
                        }
                    } else if (gp == 8 && pool8_word(w, gr, 0x1c) == 2) {
                        e = pool8_word(w, gr, 0x0a) == 0x10 ? 0x45b : 0x45d;  // 1115 / 1117
                    } else if (gp == 8) {
                        e = team != 6 ? 0x45c : 0x1f6;  // 1116 / 502
                    } else if (gp == 9) {
                        e = 0x458;  // 1112
                    } else {
                        e = team != 6 ? 0x1f7 : 0x1f6;  // 503 / 502
                    }
                } else {
                    const int f = gr >= 0 && gr < world::kShipCount ? ship_sb(S, world::pw::kEe0 + gr) : 0;
                    if (f != 0 && f < 6) {
                        e = 0x461 + f;  // 1122 ... 1126
                    } else {            // 462 "Because I'm Mad At %?." [BYTES seg3:0x2905-0x291c]
                        R = fmt(0x1ce, {briefing::FArg(gr == a ? 1 : 0), briefing::FArg(0x1cf), briefing::FArg(0x1c8)});
                    }
                }
            }
        } else if (act == 2) {
            // ---- capture or leave [BYTES seg3:0x2a9e-0x2d4a]
            const int kind = target_kind_371c(gp, gr);
            if (kind == 0) {
                if (obj_byte(w, gp, gr, 0x53) == 0 && (s != P || pm > 1)) {
                    load(0x6a4, V);  // 1700
                    e = 0x6a5;       // 1701
                } else {
                    N = object_name(w, h, 1, gp, gr, 2, N);
                    V = fmt(0x1de, {briefing::FArg(N)});  // 478 "Capturing %s"
                    const int team = world::ship_team(w, s);
                    if (order == 9 || order == 0x0c) {
                        e = ship_w(S, world::pw::kOrderBy) == a ? 0x1df : 0x1e0;  // 479 / 480
                    } else if (order == 0x0e) {
                        R = fmt(0x202, D(14, ship_w(S, world::pw::kOrderRec), 5));  // 514 "Stealing From %D"
                    } else if (gp == 0xb) {
                        e = team == 3 ? 0x1fe : team == 7 ? 0x444 : 0x1ff;  // 510 / 1092 / 511
                    } else if (gp == 6 && team == 7) {
                        e = 0x200;  // 512
                    } else if (gp == 4 && S.fuel < 300) {
                        e = 0x46b;  // 1131
                    } else if (gp == 0) {
                        e = 0x1e1;  // 481
                    } else if (gp == 4 && S.fuel < world::add16(design_tank(w, design_of(w, s)), -15)) {
                        e = 0x46a;  // 1130
                    } else if (gp == 4) {
                        e = 0x469;  // 1129
                    } else {
                        // No string: line 2 is the uninitialised [bp-0x110], empty [DESIGN ADR 0016 decision 1].
                        h.todo("seg3:0x1d0a capture line 2 for a goal pool without a string", "seg3:0x2f3e");
                    }
                }
            } else if (order != 0x16) {
                load(0x1dc, V);  // 476 "Leaving The Wave"
                bool held = false;
                for (int k = 0; k <= w.last_ship && k < world::kShipCount; ++k) {
                    const world::ShipRecord& K = w.ships[static_cast<std::size_t>(k)];
                    if (world::ship_team(w, k) == 7 && ship_sb(K, world::pw::kClass) == 1 &&
                        ship_w(K, world::pw::kOrderRec) == s) {
                        held = true;
                        break;
                    }
                }
                e = held ? 0x45e : a == 100 ? (!pg ? 0x45f : 0x461) : 0x1dd;  // 1118 / 1119 / 1121 / 477
            } else {
                // The arrest: 507 "Arresting You For " or 508 "Arresting %D For ", then the crime
                if (ship_w(S, world::pw::kOrderRec) == a)
                    load(0x1fb, V);
                else
                    V = fmt(0x1fc, D(14, ship_w(S, world::pw::kOrderRec), 5));
                e = static_cast<std::int8_t>(world::contact_words().ba163) + 0x44a;  // 1098 + crime
            }
        } else if (act == 3) {
            // ---- approach [BYTES seg3:0x2bd3-0x2f4d]
            if (gp != 14 || (pm == 1 && pg && s == P && order != 5)) {
                if (order == 7 || (s == P && gp == 8 && gr == 0)) {
                    load(0x1d6, V);  // 470 "Just Wandering."
                    if (gp == 0) {
                        e = 0x28e7;  // 10471
                    } else if (ship_w(S, world::pw::kGoalApp) == 5) {
                        e = 0x28e8;  // 10472
                    } else if (S.w26 < 0x32) {
                        // LoadString's return value becomes the line-2 id: 501 is 21 characters,
                        // so e = 21, and the output loads string 21 (absent) over the 501 text
                        // [BYTES seg3:0x2cb3-0x2cc7].
                        h.load(0x1f5, R, 0x28);
                        e = static_cast<int>(R.size());
                    } else {
                        e = 0x28e9;  // 10473
                    }
                } else {
                    V = fmt(0x1da, {briefing::FArg(N)});  // 474 "Approaching %s"
                    e = 0x1db;                             // 475
                }
            } else if (order == 7) {
                load(0x1d4, V);  // 468
                e = 0x1d5;       // 469
            } else {
                V = fmt(0x1d0, {briefing::FArg(N)});  // 464 "Sticking Near %s"
                const bool not_strategy_self = pm == 0 || s != P || !pg;
                if (gr == a && not_strategy_self) load(0x1c8, N);  // 456
                else if (not_strategy_self) load(0x1cf, N);         // 463 "Him."
                if (order == 5) {
                    if (!pg || s != P) {
                        e = ship_w(S, world::pw::kOrderBy) == s ? 0x28e2 : 0x28e1;  // 10466 / 10465
                    } else if (pm != 0) {
                        V = fmt(0x594, {briefing::FArg(N)});  // 1428 "Defending %s"
                    } else {
                        load(0x127, V);  // 295, unformatted (seg3:0x1e99 into [bp-0x98])
                    }
                } else if (order == 0x14) {
                    e = 0x1f2;  // 498
                } else if (order != 0x16) {
                    const int rel = world::relation(w, s, gr);
                    const int lim = world::add16(world::mul16(ship_sb(S, world::pw::kAllies), 0x28), 0x32);
                    if (lim < rel)
                        e = 499;
                    else if ((gr >= 0 && gr < world::kShipCount ? ship_sb(S, world::pw::kGrudge0 + gr) : 0) < 1)
                        e = 0x2904;  // 10500
                    else
                        e = 0x28e3;  // 10467
                } else {
                    if (ship_w(S, world::pw::kOrderRec) == a)
                        load(0x1fb, V);  // 507
                    else
                        V = fmt(0x1fc, D(14, ship_w(S, world::pw::kOrderRec), 5));  // 508
                    e = static_cast<std::int8_t>(world::contact_words().ba163) + 0x44a;
                }
            }
        } else if (act == 4) {
            // ---- flee [BYTES seg3:0x2f50-0x2f9a]
            if (gp == 14 && gr == a) {
                load(0x1e2, V);  // 482
                e = 0x1e3;       // 483
            } else {
                V = fmt(0x1e4, D(gp, gr, 5));  // 484 "Fleeing From %D"
                e = 0x1e5;                     // 485
            }
        } else if (act == 5) {
            load(0x1ea, V);  // 490 [BYTES seg3:0x2f9d]
            e = 0x1eb;       // 491
        } else {
            load(0x1e6, V);  // 486 [BYTES seg3:0x20dd-0x20f1]
            e = 0x1e7;       // 487
        }
    }

    // 5. The output [BYTES seg3:0x2a71-0x3048].
    if (s == P && pg && h.w9460() == 0) {
        R.clear();  // DGROUP:0x13cd
    } else if (e < 0x2711) {
        if (e != -1) h.load(e, R, 0x28);  // a missing string leaves R as it was
    } else {
        R = fmt(e - 10000, {briefing::FArg(N)});
    }
    std::string line1 = L0 + V;
    if (line1.size() > 0x3c || R.size() > 0x3c) ++st.bad_index;  // DGROUP:0x9498
    lines_31d0(st, mode, line1, R);
}

void defender_text(const world::World& w, ActivityHost& h, ActivityState& st, int a, const DefenderWords& d,
                   int mode) {
    const auto lang = [&h](int id) { return h.lang(id); };
    std::string T, O;  // [bp-0x34], [bp-0x5c]
    if (d.target_pool == 14 && d.target_rec == a)
        h.load(0x1c8, T, 0x28);  // 456 [BYTES seg3:0x307a-0x30aa]
    else
        T = object_name(w, h, 0, d.target_pool, d.target_rec, 5);
    if (d.owner_pool == 14 && d.owner_rec == a)
        h.load(0x1c8, O, 0x28);  // [BYTES seg3:0x30af-0x30e1]
    else
        O = object_name(w, h, 0, d.owner_pool, d.owner_rec, 5);
    using briefing::FArg;
    if (d.state == 10) {  // 3041 (T, O) [BYTES seg3:0x315f-0x3172]
        st.w8d38 = briefing::lang_format(h.lang(0xbe1), {FArg(T), FArg(O)}, lang);
    } else if (d.state == 15) {
        if (target_kind_371c(d.target_pool, d.target_rec) != 0)
            st.w8d38 = briefing::lang_format(h.lang(0xbdf), {FArg(O)}, lang);  // 3039 [BYTES seg3:0x3116-0x3124]
        else  // 3040 (T, target pool = 2, 2539, 2217, O) [BYTES seg3:0x312e-0x3155]
            st.w8d38 = briefing::lang_format(
                h.lang(0xbe0), {FArg(T), FArg(d.target_pool == 2 ? 1 : 0), FArg(0x9eb), FArg(0x8a9), FArg(O)}, lang);
    } else {
        st.w8d38 = briefing::lang_format(h.lang(0x8aa), {FArg(O)}, lang);  // 2218 [BYTES seg3:0x30f8-0x3102]
    }
    std::string l1, l2;  // [bp-0x98], [bp-0xd4]
    split_lines(st.w8d38, l1, l2);                         // seg3:0x318b
    if (l1.size() > 0x28 || l2.size() > 0x28) ++st.bad_index;  // seg3:0x3191-0x31b1
    lines_31d0(st, mode, l1, l2);
}

}  // namespace is::hud
