// The computer pilot's orders and reactions (src/world/pilot_orders.h). Each routine follows its
// listing as read for the M5 stage 3 fix; the readings with their addresses are in
// work/world/m5s3/fix-report.md, "Readings no verifier has checked".
#include "world/pilot_orders.h"

#include <array>
#include <string>
#include <vector>

#include "briefing/lang_format.h"
#include "world/relations.h"
#include "world/world.h"

namespace is::world {

namespace {

struct Once {
    bool ship = false, last = false, name = false, team = false, hurt = false;
};
Once g_once;

bool ship_ok(int j) { return j >= 0 && j < kShipCount; }

// A ship index from a record word: outside the table the reads see a zeroed scratch record
// [DESIGN ADR 0016 decision 2].
ShipRecord& ship_or_zero(PilotCtx& c, int j, const char* address) {
    if (ship_ok(j)) return c.s(j);
    static ShipRecord zero;
    zero = ShipRecord{};
    guard_todo(g_once.ship, "a ship index outside the ship table", address);
    return zero;
}

// DGROUP:0x9316 as a loop bound over the ship table [DESIGN ADR 0016 decision 2].
int last_ship(PilotCtx& c) {
    if (c.g.last_ship < kShipCount) return c.g.last_ship;
    guard_todo(g_once.last, "DGROUP:0x9316 past the ship table", "seg13:0x6900");
    return kShipCount - 1;
}

// A signed byte of ship record S at `off`; past the record 0 [DESIGN ADR 0016 decision 2].
std::int8_t sb_at(const ShipRecord& S, int off, const char* address) {
    if (off >= 0 && off < static_cast<int>(kShipRecordSize)) return ship_sb(S, off);
    guard_todo(g_once.hurt, "a ship byte past the ship record", address);
    return 0;
}

// seg8:0x285a from object `first` to object `second` (pushed in that order), in cells.
std::int16_t cells(const ObjectRecord& first, const ObjectRecord& second) {
    std::int16_t dx = 0, dy = 0;
    return cells_between(first, second.x, second.y, dx, dy);
}

// sext(word +0xce) + dword +0xdc (`movsx eax; add eax`) [BYTES seg13:0x7440-0x7446].
std::int32_t money(const ShipRecord& s) {
    const std::int64_t v = std::int64_t{ship_w(s, pw::kResources)} + static_cast<std::int32_t>(ship_d(s, pw::kCredit));
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(v)));
}

std::int16_t abs16(std::int16_t v) { return v < 0 ? sub16(0, v) : v; }  // seg9:0x88ba

// DGROUP:0x4eec + 2·(3t + n) with the port's 8 x 3 table; outside it -1 [DESIGN ADR 0016 decision 2].
std::int16_t* killer_slot(PilotCtx& c, int t, int n) {
    const int ix = 3 * t + n;
    if (ix >= 0 && ix < static_cast<int>(c.w.w4eec.size())) return &c.w.w4eec[static_cast<std::size_t>(ix)];
    guard_todo(g_once.team, "DGROUP:0x4eec + 2·(3·team + n) outside 8 x 3", "seg4:0x7217");
    return nullptr;
}

}  // namespace

// --- PilotHost defaults of the orders' calls out ----------------------------------------------------
namespace {
struct OnceHost {
    bool defenders = false, cloak = false, shield = false, notice = false, sound = false, arrest = false;
    bool text = false, lang = false, pilot = false, shp = false;
} g_once_host;
}  // namespace
void PilotHost::send_defenders(int, int, int) {
    guard_todo(g_once_host.defenders, "a ship's defenders sent at its target", "seg13:0x9a18");
}
std::int16_t PilotHost::cloak_toggle(int) {
    guard_todo(g_once_host.cloak, "a computer ship's cloak toggle", "seg11:0x4c72");
    return 0;
}
std::int16_t PilotHost::shield_toggle(int) {
    guard_todo(g_once_host.shield, "a computer ship's shield toggle", "seg11:0x4b00");
    return 0;
}
void PilotHost::notice(std::uint32_t) { guard_todo(g_once_host.notice, "the player's notice", "seg3:0x332e"); }
void PilotHost::sound(int, int, int, int) { guard_todo(g_once_host.sound, "a pilot's sound", "seg10:0x754a"); }
void PilotHost::arrest(int) {
    guard_todo(g_once_host.arrest, "the Enforcer's arrest order 0x16 (the law, stage 4)", "seg13:0xa984");
}
void PilotHost::order_text(int) {
    guard_todo(g_once_host.text, "the Strategy player's order text (M8)", "seg13:0x69ad");
}
std::string PilotHost::lang(int) {
    guard_todo(g_once_host.lang, "the pilot's strings", "seg4:0x71de");
    return {};
}
std::string PilotHost::pilot_name() {
    guard_todo(g_once_host.pilot, "the pilot name", "DGROUP:0x380c");
    return {};
}
std::string PilotHost::shp_name(int) {
    guard_todo(g_once_host.shp, "a MASTER.SHP name", "seg4:0x7262");
    return {};
}

// --- seg4:0x8346, the reaction word -------------------------------------------------------------
std::int16_t ship_reaction(PilotCtx& c, int i, int pool, int rec, int kind) {
    if (!ship_ok(i)) {
        guard_todo(g_once.ship, "a ship index outside the ship table", "seg4:0x8356");
        return 1;
    }
    ShipRecord& S = c.s(i);
    Pilot P(S);
    std::int16_t k = static_cast<std::int16_t>(kind);
    if (k == 3 && P.goal_act() == 1 && P.goal_pool() == pool && P.goal_rec() == rec)
        k = ship_sb(S, pw::kReaction);  // `cbw` [BYTES seg4:0x8350-0x8379]
    if (!(i == c.g.player && c.g.demo != 0)) c.host.send_defenders(i, pool, rec);  // seg13:0x9a18 [BYTES seg4:0x837c-0x8392]
    set_ship_b(S, pw::kReaction, static_cast<std::uint8_t>(k & 0xff));  // [BYTES seg4:0x8397-0x839f]
    return 1;                                                            // [BYTES seg4:0x83a3]
}

// --- seg13:0x7b40, the player did k's bidding --------------------------------------------------------
void bidding_report(PilotCtx& c, int k) {
    const int p = c.g.player;
    c.host.message(p, 14, k, 3);  // seg3:0x6ec(DGROUP:0x9646, 0xe, k, 3) [BYTES seg13:0x7b4a-0x7b55]
    if (!ship_ok(p)) {
        guard_todo(g_once.ship, "a ship index outside the ship table", "seg13:0x7b64");
        return;
    }
    Pilot P = c.p(p);
    if (c.g.play_mode == 0) P.order(0);  // [BYTES seg13:0x7b5d-0x7b6a]
    if (P.order_pool() == 9)             // [BYTES seg13:0x7b70-0x7b96]
        attitude_change(c.w, k, p, 10, 0x16, 0, 6);
    else
        attitude_change(c.w, k, p, 15, 0x41, 1, 8);
}

// --- seg4:0x79ae, the cloak and the shield ---------------------------------------------------------
void cloak_shield(PilotCtx& c, int i) {
    if (!ship_ok(i)) return;
    ShipRecord& S = c.s(i);
    Pilot P(S);
    if (P.has_cloak() == 0 && P.has_shield() == 0) return;          // [BYTES seg4:0x79bd-0x79ce]
    std::uint16_t a = 0;                                             // [bp-0xc] [BYTES seg4:0x79d2]
    std::uint16_t b = 0;                                             // [bp-0xe], written with every try
    if (P.has_shield() != 0 && ship_b(S, pw::kWing0) == 0) return;   // [BYTES seg4:0x79d7-0x79e8]
    if (P.has_cloak() != 0 && S.fuel < 100) return;                  // [BYTES seg4:0x79f0-0x7a01]
    // u [bp-0x10]: the system is down [BYTES seg4:0x7a05-0x7a1f].
    const bool down = ship_w(S, pw::kShieldStep) == 0 && P.cloaked() == 0;
    int h = -1;  // [bp-6]: `mov dword [bp-6], 0xffff` also zeroes the loop's j [bp-4] [BYTES seg4:0x7a24]
    for (int j = 0; j <= last_ship(c); ++j) {  // [BYTES seg4:0x7a2e-0x7a92]
        if (c.obj(kPoolShips, j).state != 0 || j == i) continue;
        const Pilot J = c.p(j);
        if (J.goal_pool() != 14 || J.goal_rec() != i || J.goal_act() != 1) continue;
        if (cells(c.obj(kPoolShips, j), c.obj(kPoolShips, i)) < 500) h = j;  // the last one wins
    }
    // seg11:0x4b00 for a ship with a shield, else seg11:0x4c72; the notice's high word when it took.
    const auto toggle = [&](std::uint16_t shield_id, std::uint16_t cloak_id) {
        if (P.has_shield() != 0) {
            if (c.host.shield_toggle(i) != 0) a = shield_id;
        } else if (c.host.cloak_toggle(i) != 0) {
            a = cloak_id;
        }
    };
    // The fight byte +0x13e + h; for h = -1 the byte before the table, +0x13d, which the
    // original reads as written [BYTES seg4:0x7c44-0x7c47].
    const auto fight_h = [&]() { return ship_sb(S, pw::kFight0 + h); };
    if (!down && P.goal_act() == 2) {  // a capture goal near: the system goes down [BYTES seg4:0x7a94-0x7b13]
        if (cells(c.obj(kPoolShips, i), c.obj(P.goal_pool(), P.goal_rec())) < 150) {
            toggle(0x408, 0x409);
            b = 0x410;
        }
    }
    const std::int16_t team = c.team(i);
    if (down && h == -1 && team != 3) return;  // [BYTES seg4:0x7b18-0x7b34]
    // The capture block falls through: the tests below read u as it was before it [BYTES seg4:0x7b13].
    do {
        if (!down && h == -1) {  // no hunter near: down for good unless a Pirate [BYTES seg4:0x7b38-0x7b95]
            if (c.rng.rand() % 3 == 0 && team != 3) {
                toggle(0x408, 0x409);
                b = 0x411;
                break;
            }
        }
        if (down && h != -1) {  // a hunter near: up [BYTES seg4:0x7b98-0x7c03]
            const int r = c.rng.rand() % 5;
            if ((r == 0 || fight_h() < -1) && fight_h() < 2) {
                toggle(0x406, 0x407);
                b = 0x412;
                break;
            }
        }
        if (down && team == 3 && h == -1 && P.goal_act() == 1 && P.goal_pool() == 14) {  // [BYTES seg4:0x7c06-0x7c7f]
            if (c.rng.rand() % 4 <= 2 && fight_h() < 2) {
                toggle(0x406, 0x407);
                b = 0x413;
                break;
            }
        }
        if (!down && h == -1 && P.goal_act() != 1) {  // [BYTES seg4:0x7c82-0x7ccc]
            toggle(0x408, 0x409);
            b = 0x414;
            break;
        }
        if (!down && fight_h() > 2) {  // [BYTES seg4:0x7cce-0x7d10]
            toggle(0x408, 0x409);
            b = 0x415;
        }
    } while (false);
    // seg3:0x332e(dword [bp-0xe]): a in the high word [BYTES seg4:0x7d15-0x7d27].
    if (i == c.g.player && a != 0) c.host.notice((static_cast<std::uint32_t>(a) << 16) | b);
}

// --- seg4:0x702e, the revenge recall ---------------------------------------------------------------
namespace {

// seg4:0x7114-0x75aa: i taunts j and, for a memory of at most 5, goes after it.
std::int16_t taunt(PilotCtx& c, int i, int j) {
    WaveGlobals& g = c.g;
    const std::uint32_t now = g.now;
    ShipRecord& S = c.s(i);
    Pilot P(S);
    const std::int8_t m = P.ee(j);
    const int k = m <= 5 ? 1 : 0;  // [bp-0x46] [BYTES seg4:0x711c-0x7123 seg4:0x7187]
    // The name [bp-0x2a]: one rand() % 3 [BYTES seg4:0x7128 seg4:0x718c].
    std::string name;
    bool named = true;
    if (c.rng.rand() % 3 == 0) {
        name = c.host.pilot_name();  // lstrcpy(buf, DGROUP:0x380c) [BYTES seg4:0x7137-0x7140]
    } else if (k == 1) {             // by the enemies byte +0x5e [BYTES seg4:0x7148-0x7185]
        const std::int8_t cl = P.cls();
        if (cl == 0) name = c.host.lang(0x29c);       // 668
        else if (cl < 3) name = c.host.lang(0x29d);   // 669
        else if (cl == 3) name = c.host.lang(0x29e);  // 670
        else named = false;
    } else {  // by the allies byte +0x5d [BYTES seg4:0x719b-0x71d4]
        const std::int8_t al = P.allies();
        if (al == 0) name = c.host.lang(0x29f);       // 671
        else if (al < 2) name = c.host.lang(0x2a0);   // 672
        else if (al == 3) name = c.host.lang(0x2a1);  // 673
        else named = false;
    }
    if (!named) {
        // [bp-0x2a] is not written for a byte outside the cases: the port uses the empty text
        // [DESIGN ADR 0016] [BYTES seg4:0x717c seg4:0x71ce].
        static bool once = false;
        guard_todo(once, "seg4:0x702e's taunt name unwritten for a byte outside the cases (empty)", "seg4:0x71e3");
    }
    // A memory of 4 (a killer): the team's killer slots name j [BYTES seg4:0x71e3-0x7288].
    const std::int16_t ti = c.team(i);
    std::int16_t found = 0;  // [bp-0x48]
    std::string killed;      // [bp-0x3e]
    std::int16_t ago = 0;    // [bp-0x44]: unwritten unless found, and then read only by a group that is off
    ContactWords& cw = contact_words();
    if (m == 4) {
        for (int n = 0; n < 3; ++n) {
            const std::int16_t* slot = killer_slot(c, ti, n);
            if (!slot || *slot != c.s(j).roster_index) continue;
            found = 1;
            const std::size_t ix = static_cast<std::size_t>(3 * ti + n);
            // seg14:0x3eda when MASTER.SHP is not in memory (DGROUP:0x6e20 = 0) and seg14:0x4074
            // after the format: the port's host reads the file for the name [BYTES seg4:0x7222-0x722f seg4:0x72f8-0x7304].
            killed = c.host.shp_name(cw.w6b10[ix]);  // far DGROUP:0x6e20 + 0x41·DGROUP:0x6b10[ix] + 0x0a
            ago = sub16(c.w.wave, cw.w4fd0[ix]);     // DGROUP:0x91f8 - DGROUP:0x4fd0[ix] [BYTES seg4:0x7267-0x727e]
        }
    }
    std::int16_t remember = 0;  // [bp-0x4c] [BYTES seg4:0x728c-0x72a2]
    if (found == 0) remember = (c.rng.rand() & 1) == 0 ? 1 : 0;
    const std::int16_t hey = (c.rng.rand() & 1) == 0 ? 1 : 0;  // [BYTES seg4:0x72d7-0x72e6]
    // seg10:0xa332(DGROUP:0x8d38, 3033, 1, hey, name, remember, !found, 849 + m, found, killed,
    // ago, 2518): "%<Hey! %>%s! %<I Remember That %>You %<%S%>%<Killed a %s %# Ago!%>"
    // [BYTES seg4:0x72a7-0x72f0] [BYTES ENG_LANG.DLL STRING 190] id 3033.
    using briefing::FArg;
    const briefing::LangLookup lang = [&c](int id) { return c.host.lang(id); };
    briefing::lang_format_into(c.pg.taunt, 0xbd9, 1,
                               {FArg(hey), FArg(name), FArg(remember), FArg(found == 0 ? 1 : 0),
                                FArg(add16(static_cast<std::int16_t>(m), 0x351)), FArg(found), FArg(killed),
                                FArg(ago), FArg(0x9d6)},
                               lang);
    c.host.message(j, kPoolShips, i, 0x4f + k);  // [BYTES seg4:0x7309-0x7318]
    if (k != 0) {
        const std::int16_t rv = P.revenge(j);
        if (rv < 9 || ti == 2) {  // [BYTES seg4:0x7328-0x7397]
            P.order(0);
            P.goal_pool(14);
            P.est_pool(14);
            P.goal_rec(static_cast<std::int16_t>(j));
            P.est_rec(static_cast<std::int16_t>(j));
            P.replan_at(now + 8000u);
            P.est_act(1);  // dword +0x36 = 0x10001
            P.goal_act(1);
            P.est_app(2);  // dword +0x3a = 0x20002
            P.goal_app(2);
            if (ti == 2) {  // the FUZZY ONES go together [BYTES seg4:0x739f-0x74a0]
                for (int n = 0; n <= last_ship(c); ++n) {
                    Pilot M = c.p(n);
                    if (c.team(n) == 2 && n != j) {
                        M.est_act(3);
                        M.goal_act(3);
                        M.est_app(2);
                        M.goal_app(2);
                        M.order(0);
                        M.goal_pool(14);
                        M.est_pool(14);
                        M.goal_rec(static_cast<std::int16_t>(j));
                        M.est_rec(static_cast<std::int16_t>(j));
                        M.replan_at(now + 15000u);
                    }
                    // A team-0 ship takes command 2 against j when i's memory is 1 or 3; the
                    // target words are i's own order words [BYTES seg4:0x7407-0x749a].
                    if (c.team(n) == 0 && (P.ee(j) == 3 || P.ee(j) == 1)) {
                        M.order(2);
                        M.order_until(now + 20000u);
                        M.order_pool(14);
                        M.order_rec(static_cast<std::int16_t>(j));
                        M.order_by(static_cast<std::int16_t>(i));
                        M.one_shot(1);
                        c.host.target(i, P.order_pool(), P.order_rec());  // seg3:0xf4(i, i +0x54, i +0x56)
                        ship_reaction(c, n, 14, j, 8);                    // seg4:0x8346(n, 14, j, 8)
                        M.ee(j, 3);
                        M.revenge(j, 0);
                    }
                }
            } else {
                ship_reaction(c, i, 14, j, 8);  // [BYTES seg4:0x74a3-0x74ae]
            }
        } else {  // the commands 2-4 by the word's size [BYTES seg4:0x74b7-0x754d]
            if (rv < 14) P.order(2);
            else if (rv < 18) P.order(3);
            else P.order(4);
            ship_reaction(c, i, 14, j, 8);
            P.order_until(now + 20000u);
            P.order_pool(14);
            P.order_rec(static_cast<std::int16_t>(j));
            P.order_by(static_cast<std::int16_t>(i));
            P.one_shot(1);
            c.host.target(i, 14, j);  // seg3:0xf4(i, +0x54, +0x56)
        }
        if (P.ee(j) == 4) {  // the killer is forgotten by the team [BYTES seg4:0x7550-0x7595]
            for (int n = 0; n < 3; ++n) {
                std::int16_t* slot = killer_slot(c, ti, n);
                if (slot && *slot == c.s(j).roster_index) *slot = -1;
            }
        }
    }
    P.revenge(j, 0);  // [BYTES seg4:0x7597-0x75a4]
    return 1;
}

}  // namespace

std::int16_t revenge_recall(PilotCtx& c, int i) {
    if (!ship_ok(i)) return 0;
    WaveGlobals& g = c.g;
    ShipRecord& S = c.s(i);
    Pilot P(S);
    const int pl = g.player;
    if (!ship_ok(pl)) return 0;
    if (P.ee(pl) == 0 || P.revenge(pl) == 0) return 0;                 // [BYTES seg4:0x7044-0x7064]
    if (g.demons_gate != 0 || g.wave_kind == 7 || g.wave_kind == 6) return 0;  // [BYTES seg4:0x706b-0x707d]
    std::int16_t w0 = ship_w(S, pw::kCountdown);
    if (w0 != 0 && w0 < 4) set_ship_w(S, pw::kCountdown, sub16(w0, 1));  // [BYTES seg4:0x707f-0x7091]
    if (i == pl && g.demo == 0) return 0;                              // [BYTES seg4:0x7095-0x70a2]
    if (c.obj(kPoolShips, i).on_screen == 0) return 0;                 // [BYTES seg4:0x70a4-0x70b1]
    w0 = ship_w(S, pw::kCountdown);
    if (w0 != 0) {  // [BYTES seg4:0x70b3-0x70e0]
        if (w0 == 4) c.host.message(pl, kPoolShips, i, 0x88);  // 1424 "Now I'm Back In The Thick Of Things. Game On!"
        set_ship_w(S, pw::kCountdown, sub16(w0, 1));
        return 0;
    }
    if (S.w26 < 0x2d) return 0;  // [BYTES seg4:0x70e2-0x70ec]
    const std::int16_t ti = c.team(i);
    if (ti == 4) return 0;        // a KNIGHT forgives [BYTES seg4:0x70f0-0x7100]
    if (c.w.w934e < 7) return 0;  // the wave's first 7 s [BYTES seg4:0x7102-0x7107]
    const int last = last_ship(c);
    for (int j = 0; j < g.last_ship && j <= last; ++j) {  // `jge` on DGROUP:0x9316 [BYTES seg4:0x75ac-0x75b5]
        if (c.obj(kPoolShips, j).on_screen == 0 || j == i) continue;  // [BYTES seg4:0x75b9-0x75ce]
        if (!(P.revenge(j) > 5)) continue;                            // [BYTES seg4:0x75d0-0x75de]
        if (ti == 7) continue;                                        // [BYTES seg4:0x75e0-0x75ef]
        if (c.team(j) != ti) return taunt(c, i, j);                   // [BYTES seg4:0x75f1-0x7604]
        const std::int8_t m = P.ee(j);
        if (m > 8 || m == 4) return taunt(c, i, j);                   // [BYTES seg4:0x7608-0x761b]
        P.ee(j, 0);                                                   // a teammate is forgiven [BYTES seg4:0x761e-0x7627]
        P.revenge(j, 0);
        return 0;
    }
    return 0;
}

// --- seg13:0x6900, the order executor --------------------------------------------------------------
namespace {

// seg13:0xad53-0xad80 and its copies: message 0xb7 to the player and the estimate (12, 0) with
// +0x3a = +0x36 = 2, leave by gate 0 [BYTES seg13:0xad53-0xad7c].
void leave_gate0(PilotCtx& c, int e) {
    Pilot E = c.p(e);
    c.host.message(c.g.player, kPoolShips, e, 0xb7);
    E.est_pool(12);  // `mov dword [+0x06], 0xc`
    E.est_rec(0);
    E.est_app(2);
    E.est_act(2);
}

// The order's acknowledgement: +0xed = 1 is cleared and message 1 goes to the order's giver
// +0x58 [BYTES seg13:0x713e-0x7155].
void acknowledge(PilotCtx& c, int i) {
    Pilot P = c.p(i);
    if (P.one_shot() != 1) return;
    P.one_shot(0);
    c.host.message(P.order_by(), kPoolShips, i, 1);
}

// Order 1 [BYTES seg13:0x69e3-0x6bc3].
void order_help(PilotCtx& c, int i) {
    const std::uint32_t now = c.g.now;
    Pilot P = c.p(i);
    const std::int16_t k = help_answer(c, i, P.order_rec());  // seg13:0x78b8(i, +0x56) [bp-0x1e]
    if (k >= 0) {  // j's attacker k [BYTES seg13:0x6a0f-0x6b4c]
        P.est_pool(14);
        P.est_rec(k);
        P.est_act(1);
        P.est_app(2);
        if (P.one_shot() != 1) return;
        P.one_shot(0);
        attitude_change(c.w, P.order_rec(), i, 0x14, 0x3c, 1, 8);  // seg13:0x2714(+0x56, i, 20, 60, 1, 8)
        c.host.message(P.order_rec(), kPoolShips, i, 1);
        if (c.team(i) == 4) {  // a KNIGHT [BYTES seg13:0x6a5d-0x6b17]
            // The block at seg13:0x6a98 needs i's team to differ from 4 inside the team-4 branch,
            // so it never runs; it is written as the bytes have it [BYTES seg13:0x6a8f-0x6a96].
            const std::int8_t m = P.ee(k);
            if (m != 0 && m <= 5 && P.revenge(k) > 3 && c.team(i) != 4) {
                c.host.message(k, kPoolShips, i, 0x7f);
                P.order(4);
                ship_reaction(c, i, 14, k, 0xb);
                P.replan_at(now + 500u);
                P.order_pool(14);
                P.order_rec(k);
                P.one_shot(1);
                P.revenge(k, 0);
            } else {
                c.host.message(k, kPoolShips, i, 0x80);  // 1417 "From Your Perspective, Knights Are Not So Knice!"
            }
            attitude_change(c.w, i, k, 0, -140, 0, 3);  // seg13:0x2714(i, k, 0, -140, 0, 3) [BYTES seg13:0x6b00-0x6b0f]
        } else {
            c.host.message(k, kPoolShips, i, 0x81);  // 1418 "This Ones For My Buddy, Who You Just Attacked."
        }
        if (P.order() == 4) return;          // [BYTES seg13:0x6b2c-0x6b36]
        ship_reaction(c, i, 14, k, 0xe);     // seg4:0x8346(i, 14, k, 14)
        return;
    }
    if (k < -1) {  // a refusal 0xaf ... 0xb3 (1920-1924) [BYTES seg13:0x6b4f-0x6b79]
        c.host.message(P.order_rec(), kPoolShips, i, sub16(0xad, k));
        P.order(0);
        return;
    }
    // -1: nobody to fight, order 5 (follow) [BYTES seg13:0x6b7c-0x6bc3]
    P.order(5);
    P.replan_at(now + 200u);
    c.host.message(P.order_by(), kPoolShips, i, 1);
    c.host.message(P.order_rec(), kPoolShips, i, 1);
    P.one_shot(0);
}

// Orders 2-4, the commands [BYTES seg13:0x6bc6-0x6da6].
void order_command(PilotCtx& c, int i) {
    Pilot P = c.p(i);
    ShipRecord& S = c.s(i);
    // [bp-0x1a], written only for a ship target; a stale word otherwise is read at seg13:0x6c70
    // where it cannot change the outcome (the refusal also needs +0x54 = 14), so 0 [DESIGN].
    std::int16_t r = 0;
    if (P.order_pool() == 14) r = c.host.strength(i, P.order_rec());  // seg4:0x5d9c(i, +0x56) [BYTES seg13:0x6bdd-0x6bf3]
    bool refuse = false;
    if (P.order_pool() == 14 && (r <= 15 || r >= 600) && P.order() != 4 && !(i == c.g.player && c.g.demo != 0) &&
        P.cls() >= 2)
        refuse = true;  // [BYTES seg13:0x6bf6-0x6c34]
    if (!refuse) {      // [BYTES seg13:0x6c38-0x6c92]
        const int t = P.order_rec();
        if (c.obj(P.order_pool(), t).state == 1) refuse = true;
        else if (P.order_pool() == 14 && ship_or_zero(c, t, "seg13:0x6c63").bec == 0) refuse = true;
        else if (r <= 0x4b && sb_at(S, pw::kFight0 + t, "seg13:0x6c7f") <= -6 && P.order() < 3 && P.order_pool() == 14)
            refuse = true;
    }
    if (!refuse) {  // [BYTES seg13:0x6c94-0x6d01]
        P.est_pool(P.order_pool());
        P.est_rec(P.order_rec());
        P.est_act(1);
        P.est_app(2);
        if (P.one_shot() != 1) return;
        P.one_shot(0);
        if (P.order_by() == i && P.order_pool() == 14 && P.order_rec() == c.g.player)
            c.host.sound(0x3f, 0, 0, 0);  // seg10:0x754a(0x3f, 0, 0, 0) [BYTES seg13:0x6cdc-0x6ce4]
        else
            c.host.message(P.order_by(), kPoolShips, i, 1);
        return;
    }
    // The refusal [BYTES seg13:0x6d04-0x6da6].
    if (P.order_pool() == 14) {
        const std::int16_t lim = sub16(static_cast<std::int16_t>(P.cls()), 7);
        const int t = P.order_rec();
        if (sb_at(S, pw::kFight0 + t, "seg13:0x6d1e") < lim) {
            const std::int16_t s = c.host.strength(i, t);  // seg4:0x5d9c(i, +0x56)
            // `mov al, 0x14; imul byte [+0x5d]`: 20·sbyte +0x5d in AX, + 15 [BYTES seg13:0x6d37-0x6d3d]
            const std::int16_t v = add16(static_cast<std::int16_t>(20 * P.allies()), 15);
            if (v > s) {
                if (P.order_by() == i)
                    c.host.message(P.order_rec(), kPoolShips, i, 0x8a);  // 1426
                else
                    c.host.message(P.order_by(), kPoolShips, i, 0x73);
                if (ship_ok(t)) {
                    P.fight(t, 0);     // [BYTES seg13:0x6d77]
                    P.grudge(t, -1);   // 0xff [BYTES seg13:0x6d80]
                } else {
                    guard_todo(g_once.hurt, "a ship byte past the ship record", "seg13:0x6d77");
                }
                P.order(0);  // seg13:0x78a1
                return;
            }
        }
    }
    P.order(6);  // then seg13:0x78a1 clears it again [BYTES seg13:0x6d88-0x6da6]
    c.host.message(P.order_by(), kPoolShips, i, 0x13);
    P.order(0);
}

// Order 5, follow the order's ship [BYTES seg13:0x6da9-0x6f82].
void order_follow(PilotCtx& c, int i) {
    const std::uint32_t now = c.g.now;
    Pilot P = c.p(i);
    const std::int16_t d = cells(c.obj(kPoolShips, i), c.obj(P.order_pool(), P.order_rec()));  // [bp-4]
    P.est_app(d < 0x15e ? 3 : 2);
    P.est_pool(P.order_pool());
    P.est_rec(P.order_rec());
    P.est_act(3);
    P.replan_at(now + 700u);
    const ShipRecord& T = ship_or_zero(c, P.order_rec(), "seg13:0x6e37");
    if (T.w26 > 40 && P.order_until() < now && !(i == c.g.player && c.g.play_mode != 0 && c.g.demo != 0)) {
        c.host.message(P.order_by(), kPoolShips, i, 0x13);  // [BYTES seg13:0x6e65-0x6e77]
        P.order(0);
        if (P.called_for() == P.order_rec() && i != c.g.player) {  // [BYTES seg13:0x6e85-0x6eb3]
            P.order(10);
            P.replan_at(now + 500u);
            return;
        }
    }
    if (ship_or_zero(c, P.order_rec(), "seg13:0x6ebb").bec == 0) {  // [BYTES seg13:0x6eb6-0x6ee3]
        P.order(0);
        P.replan_at(now + 500u);
        return;
    }
    const std::int16_t k = help_answer(c, i, P.order_rec());  // seg13:0x78b8(i, +0x56) [BYTES seg13:0x6ee6-0x6efb]
    if (k > -1) {
        P.est_rec(k);
        P.est_act(1);
        P.est_app(2);
        P.order(1);
        P.one_shot(1);
        P.replan_at(now + 700u);
        return;
    }
    if (k < -1) {  // [BYTES seg13:0x6f32-0x6f82]
        c.host.message(P.order_rec(), kPoolShips, i, sub16(0xad, k));
        P.order(0);
        if (P.called_for() == P.order_rec() && i != c.g.player) P.order(10);
        P.replan_at(now + 500u);
    }
}

// Order 7, hold near the nearest ship [BYTES seg13:0x6f85-0x7128].
void order_hold(PilotCtx& c, int i) {
    const std::uint32_t now = c.g.now;
    Pilot P = c.p(i);
    if (P.one_shot() == 1 && P.order_by() != i) {
        P.one_shot(0);
        c.host.message(P.order_by(), kPoolShips, i, 1);
    }
    P.replan_at(now + 2000u);
    std::int16_t best = 20000, bp = -1, br = 0;  // [bp-0xe], [bp-0x10], [bp-0x12]
    const int pool = kPoolShips;                  // [bp-6] = 14, never changed
    for (int k = 0; k < c.pools.in_use(pool); ++k) {  // [BYTES seg13:0x6fea-0x7086]
        if (pool == 14 && k == i) continue;
        const ObjectRecord& O = c.obj(pool, k);
        if (O.state != 0 || O.mass == 0) continue;
        std::int16_t d = cells(c.obj(kPoolShips, i), O);
        if (pool == 14 && c.host.relation(k, i) < 100) d = sub16(d, 0x96);  // seg13:0x2688(k, i)
        if (best > d) {
            best = d;
            bp = static_cast<std::int16_t>(pool);
            br = static_cast<std::int16_t>(k);
        }
    }
    if (bp != -1) {  // [BYTES seg13:0x708a-0x70e7]
        P.est_pool(bp);
        P.est_rec(br);
        P.est_app(5);
        if (best < 0x12c) {
            P.est_act(3);
        } else {
            const ObjectRecord& B = c.obj(kPoolShips, br);
            if (abs16(B.vx) > 0x1e || abs16(B.vy) > 0x1e) {
                c.host.thrust(i, 5);  // seg11:0x5ab6(i, 5), the brake
                P.est_act(5);
            }
        }
    } else {
        P.est_act(5);  // [BYTES seg13:0x70ec]
    }
    if (!(P.order_until() < now)) return;  // [BYTES seg13:0x70f7-0x7111]
    if (i == c.g.player && c.g.demo != 0) return;
    P.order(0);
}

// Order 9, run from the hunters [BYTES seg13:0x712b-0x727b].
void order_flee(PilotCtx& c, int i) {
    const std::uint32_t now = c.g.now;
    Pilot P = c.p(i);
    acknowledge(c, i);
    std::int16_t best = 20000, n = 0;  // [bp-0x16], [bp-0x1c]
    for (int j = 0; j <= last_ship(c); ++j) {  // [BYTES seg13:0x716c-0x71c0]
        if (c.obj(kPoolShips, j).state != 0) continue;
        const Pilot J = c.p(j);
        if (J.goal_pool() != 14 || J.goal_rec() != i || J.goal_act() != 1) continue;
        const std::int16_t s = c.host.strength(i, j);  // seg4:0x5d9c(i, j)
        if (s < best) best = s;
        n = add16(n, 1);
    }
    if ((n != 0 && best < 0x46) || n > 1) {  // [BYTES seg13:0x71c2-0x7212]
        if (!(c.g.play_mode != 0 && c.g.demo != 0 && i == c.g.player)) {
            P.order_pool(0);
            P.order(0);
            P.replan_at(now);
            c.host.message(P.order_by(), kPoolShips, i, 0x21);
        }
    }
    if (c.obj(P.order_pool(), P.order_rec()).state != 0) {  // [BYTES seg13:0x7215-0x7256]
        c.host.message(P.order_by(), kPoolShips, i, 0x22);
        P.order(0);
        P.replan_at(now);
    }
    P.est_act(2);
    P.est_app(2);
    P.est_pool(P.order_pool());
    P.est_rec(P.order_rec());
}

// The nearest gate's beam record, 16-bit compares, no message [BYTES seg13:0x7553-0x75bc].
void nearest_gate(PilotCtx& c, int i, bool say) {
    Pilot P = c.p(i);
    std::int16_t best = 20000;
    for (int k = 0; k < 2 * c.g.gates; k += 2) {
        const std::int16_t d = cells(c.obj(kPoolShips, i), c.obj(kPoolGateCentres, k));
        if (!(d < best)) continue;
        best = d;
        if (say) c.host.message(c.g.player, kPoolShips, i, 0xb7);  // [BYTES seg13:0x72fb-0x7307]
        P.est_pool(12);
        P.est_rec(static_cast<std::int16_t>(k));
        P.est_act(2);
        P.est_app(2);
    }
}

// Order 0x0d, the needs: repairs, fuel, the hunters [BYTES seg13:0x73fe-0x776f].
void order_needs(PilotCtx& c, int i) {
    const std::uint32_t now = c.g.now;
    Pilot P = c.p(i);
    ShipRecord& S = c.s(i);
    bool search = true;  // seg13:0x745c
    if (P.one_shot() == 1) {
        P.one_shot(0);
        c.host.message(P.order_by(), kPoolShips, i, 1);
        bool go = true;  // to seg13:0x746e
        if ((S.w26 < 0x19 || S.w2a < 0x19) && money(S) > 0x12c) {  // [BYTES seg13:0x742d-0x7456]
            c.host.ambulance_call(i);  // seg16:0xb4e(i), its result unread
            go = false;
        }
        if (go && (add16(shl16(static_cast<std::int16_t>(P.cls()), 2), 0x1e) > S.w26 || S.w2a < 0x1d) &&
            P.resources() < 0xc8) {  // [BYTES seg13:0x746e-0x7496]
            search = false;
            std::int16_t bj = -1;  // [bp-0x18]
            if (P.goal_act() != 4 && c.w.w932c != 0) {  // [BYTES seg13:0x749d-0x74f4]
                for (int j = 0; j <= last_ship(c); ++j) {
                    if (c.obj(kPoolShips, j).state != 0) continue;
                    const Pilot J = c.p(j);
                    if (J.goal_pool() != 14 || J.goal_rec() != i || J.goal_act() != 1) continue;
                    bj = static_cast<std::int16_t>(j);
                    break;
                }
            }
            if (bj != -1) {  // defend against the first hunter, order 7 [BYTES seg13:0x74fd-0x7548]
                P.goal_pool(14);
                P.est_pool(14);
                P.goal_rec(bj);
                P.est_rec(bj);
                P.est_app(5);  // dword +0x3a = 0x50005
                P.goal_app(5);
                P.est_act(4);  // dword +0x36 = 0x40004
                P.goal_act(4);
                P.order(7);
                P.replan_at(now + 4000u);
                c.host.message(bj, kPoolShips, i, 0x47);  // 650 "Leave Me Be! ..."
                return;
            }
            nearest_gate(c, i, false);  // [BYTES seg13:0x7553-0x75bc]
            P.replan_at(now + 500u);
            if (c.team(i) == c.team(c.g.player)) c.host.message(c.g.player, kPoolShips, i, 0x59);  // [BYTES seg13:0x75d8-0x7601]
            return;
        }
    }
    if (!search) return;
    // The nearest hunter, then the nearest defender sent at i [BYTES seg13:0x745c-0x76e4].
    std::int16_t best = 20000, bp = -1, br = 0;
    for (int j = 0; j <= last_ship(c); ++j) {
        if (c.obj(kPoolShips, j).state != 0) continue;
        const Pilot J = c.p(j);
        if (J.goal_act() != 1 || J.goal_pool() != 14 || J.goal_rec() != i) continue;
        const std::int16_t d = cells(c.obj(kPoolShips, i), c.obj(kPoolShips, j));
        if (d < best) {
            bp = 14;
            br = static_cast<std::int16_t>(j);
            best = d;
        }
    }
    for (int k = 0; k < c.pools.in_use(2); ++k) {
        if (c.obj(2, k).state != 0) continue;
        if (c.host.defender_word(k, 0) != 15 || c.host.defender_word(k, 0x0a) != 14 || c.host.defender_word(k, 0x0c) != i)
            continue;
        const std::int16_t d = cells(c.obj(kPoolShips, i), c.obj(2, k));
        if (d < best) {
            bp = 2;
            br = static_cast<std::int16_t>(k);
            best = d;
        }
    }
    if (bp != -1) {  // [BYTES seg13:0x76e6-0x7720]
        P.est_pool(bp);
        P.est_rec(br);
        P.est_act(1);
        P.est_app(2);
        P.replan_at(now + 5000u);
        ship_reaction(c, i, bp, br, 0xf);
        return;
    }
    if (P.order_by() != i) c.host.message(P.order_by(), kPoolShips, i, 0x39);  // 452 [BYTES seg13:0x7723-0x7739]
    P.order(7);
    P.replan_at(now + 1500u);
    P.one_shot(1);
    P.order_until(now + 20000u);
}

// Order 0x0e, the race for a capture [BYTES seg13:0x7772-0x784b].
void order_race(PilotCtx& c, int i) {
    const std::uint32_t now = c.g.now;
    Pilot P = c.p(i);
    const int t = P.order_rec();
    const Pilot T(ship_or_zero(c, t, "seg13:0x7780"));
    bool drop = T.alive() == 0 || c.obj(kPoolShips, t).state != 0 || T.goal_act() == 1;
    if (!drop && T.goal_act() != 2 && !(t == c.g.player && c.g.play_mode == 0)) drop = true;  // [BYTES seg13:0x77ad-0x77bf]
    if (drop) {  // [BYTES seg13:0x7833-0x784b]: +0x94 = now, the dx:ax of the +0x98 store
        P.order(0);
        P.order_until(now);
        P.replan_at(now);
        return;
    }
    if (T.goal_act() == 2 || c.obj(T.goal_pool(), T.goal_rec()).b53 != 0) {  // [BYTES seg13:0x77c1-0x780f]
        P.est_pool(T.goal_pool());
        P.est_rec(T.goal_rec());
        P.est_act(2);
        P.est_app(2);
        return;
    }
    P.est_pool(14);  // follow the player [BYTES seg13:0x7812-0x782b]
    P.est_rec(static_cast<std::int16_t>(t));
    P.est_act(3);
    P.est_app(2);
}

}  // namespace

void order_executor(PilotCtx& c, int i) {
    if (!ship_ok(i)) {
        guard_todo(g_once.ship, "a ship index outside the ship table", "seg13:0x691c");
        return;
    }
    WaveGlobals& g = c.g;
    const std::uint32_t now = g.now;
    Pilot P = c.p(i);
    P.replan_at(now + 4000u);  // [BYTES seg13:0x690f-0x6925]
    const std::int16_t team = c.team(i);
    if (team == 7) {  // [BYTES seg13:0x6929-0x693d]
        enforcer_orders(c, i);
        return;
    }
    if (team == 6 && P.order_by() != i && !(i == g.player && g.demo != 0)) {  // a RENEGADE takes no order [BYTES seg13:0x6940-0x69e0]
        P.order(0);
        c.host.message(P.order_by(), kPoolShips, i, 0x83);
        P.replan_at(now);
        P.one_shot(0);
        if (g.demo == 0 && i == g.player) c.host.order_text(i);
        return;
    }
    switch (P.order()) {
    case 1: order_help(c, i); return;
    case 2: case 3: case 4: order_command(c, i); return;
    case 5: order_follow(c, i); return;
    case 7: order_hold(c, i); return;
    case 9: order_flee(c, i); return;
    case 10:  // leave by the nearest gate [BYTES seg13:0x727e-0x732c]
        acknowledge(c, i);
        nearest_gate(c, i, true);
        return;
    case 0x0c: {  // fetch the nearest fuel tank [BYTES seg13:0x732e-0x73fb]
        acknowledge(c, i);
        std::int16_t best = 20000, bk = -1;
        for (int k = 0; k < c.pools.in_use(4); ++k) {
            if (c.obj(4, k).state != 0) continue;
            const std::int16_t d = cells(c.obj(kPoolShips, i), c.obj(4, k));
            if (d < best) {
                best = d;
                bk = static_cast<std::int16_t>(k);
            }
        }
        if (bk >= 0) {
            P.order_pool(4);
            P.est_pool(4);
            P.order_rec(bk);
            P.est_rec(bk);
            P.est_act(2);
            P.est_app(2);
            return;
        }
        c.host.message(P.order_by(), kPoolShips, i, 0x38);  // 448 "I Can't Find A Tank Of Gas Anywhere!"
        P.order(0);
        return;
    }
    case 0x0d: order_needs(c, i); return;
    case 0x0e: order_race(c, i); return;
    default: break;
    }
    // Any other order [BYTES seg13:0x784e-0x78a6].
    if (i == g.player && g.demo != 0) {
        if (c.host.visible(P.target_pool(), P.target_rec()) != 0) {  // seg3:0x0(+0x3e, +0x40)
            P.est_pool(P.target_pool());
            P.est_rec(P.target_rec());
        }
        P.order(7);
        P.replan_at(now);
        P.est_act(5);
        return;
    }
    P.order(0);
}

// --- seg13:0xa6bc, the Enforcer's orders -----------------------------------------------------------
void enforcer_orders(PilotCtx& c, int e) {
    if (!ship_ok(e)) return;
    WaveGlobals& g = c.g;
    const std::uint32_t now = g.now;
    ShipRecord& S = c.s(e);
    Pilot E(S);
    E.replan_at(now + 750u);  // [BYTES seg13:0xa6cb-0xa6e1]
    // 1. A doughnut in flight first, while the hook holds nothing [BYTES seg13:0xa6e5-0xa78b].
    if (c.pools.in_use(6) != 0 && c.obj(kPoolShips, e).w36 == -1) {
        for (int k = 0; k < c.pools.in_use(6) && k < kMissileSlots; ++k) {
            if (c.pools.missile(k).weapon != 0x10 || c.obj(6, k).state != 0) continue;
            E.est_pool(6);
            E.est_rec(static_cast<std::int16_t>(k));
            E.est_app(2);
            E.est_act(2);
            if (c.obj(kPoolShips, e).w36 != 14) return;
            // The HANDCUFFS throw of seg13:0xa708: object +0x36 was -1 above, so it never runs here.
            for (int s = 0; s <= ship_sb(S, pw::kSlots); ++s)
                if (sb_at(S, 0xb8 + s, "seg13:0xa71e") == 15) {
                    c.host.weapon_use(s, e, 0);  // seg4:0x584c(k, e, 0)
                    return;
                }
            return;
        }
    }
    const int order = E.order();
    if (order == 0x14) {  // 2. The patrol [BYTES seg13:0xa78e-0xa921]
        bool gone = false;  // [bp-0x14]
        const int w0 = E.order_rec();
        if (c.obj(E.order_pool(), w0).state != 0) gone = true;
        const Pilot W0(ship_or_zero(c, w0, "seg13:0xa7ca"));
        if (W0.alive() == 0) gone = true;
        if (W0.goal_act() != 1 && E.order_until() < now) gone = true;  // [BYTES seg13:0xa7dc-0xa7f7]
        else if (E.order_until() < static_cast<std::uint32_t>(now + 5000u)) gone = true;  // [BYTES seg13:0xa7f9-0xa817]
        if (gone) {  // the next live non-Enforcer in play after +0x56, cyclic [BYTES seg13:0xa826-0xa8c4]
            const int last = last_ship(c);
            int w = w0;
            for (int m = 0; m <= last; ++m) {
                w = w + 1;
                if (w > last) w = 0;
                if (c.obj(kPoolShips, w).state != 0 || c.s(w).bec == 0 || c.team(w) == 7) continue;
                break;
            }
            E.order_pool(14);
            E.order_rec(static_cast<std::int16_t>(w));
            E.order_until(now + 13000u);
            if (c.obj(kPoolShips, w).state != 0 || ship_or_zero(c, w, "seg13:0xa8ac").bec == 0 || c.team(w) == 7) {
                leave_gate0(c, e);  // seg13:0xad53
                return;
            }
        }
        E.est_pool(E.order_pool());  // [BYTES seg13:0xa8c7-0xa8e2]
        E.est_rec(E.order_rec());
        E.est_app(2);
        E.est_act(3);
        const Pilot W(ship_or_zero(c, E.order_rec(), "seg13:0xa8e8"));
        if (W.goal_pool() != 14 || W.goal_rec() != e || W.goal_act() != 1) return;
        E.est_act(1);  // the watched ship hunts e [BYTES seg13:0xa90a-0xa919]
        ship_reaction(c, e, 14, E.order_rec(), 0x12);
        return;
    }
    if (order == 0x15) {  // 3. The warning [BYTES seg13:0xa924-0xa987]
        if (E.one_shot() == 1) {
            c.host.message(E.order_rec(), kPoolShips, e, 0x3b);  // 550 "Do Not Provoke Me! ..."
            E.one_shot(0);
        } else {
            E.order(0x14);
            E.order_until(now + 5000u);
        }
        E.replan_at(now + 2000u);
        return;
    }
    if (order == 0x16) {  // 4. The arrest: the law's (ruling R3) [BYTES seg13:0xa98a-0xac34]
        c.host.arrest(e);
        return;
    }
    // 5. Anything else: terminate [BYTES seg13:0xac37-0xadac].
    E.order(0x17);
    const int t = E.order_rec();  // [bp-8]
    if (E.one_shot() != 0) {
        bool give_up = E.one_shot() == 2;
        if (!give_up)  // [BYTES seg13:0xac59-0xac71]
            give_up = sb_at(S, pw::kHurt0 + 9 * t, "seg13:0xac5c") > 2 &&
                      sb_at(S, pw::kFight0 + t, "seg13:0xac65") < -2 && S.w26 < 0x37;
        if (give_up) {  // [BYTES seg13:0xac73-0xacb2]
            c.host.message(g.player, kPoolShips, e, 0xb7);
            E.est_pool(12);
            E.est_rec(0);
            E.est_app(2);
            E.est_act(2);
            E.one_shot(0);
            c.host.call_in(t, 0x14, 0x58);  // seg13:0xadbe(t, 0x14, 0x58): a Terminator
            return;
        }
        c.host.message(t, kPoolShips, e, 0x3e);  // 553 "Alright Tough Guy, I'll Resort To Force." [BYTES seg13:0xacb5-0xacc2]
        E.one_shot(0);
        c.host.target(e, E.order_pool(), E.order_rec());  // seg3:0xf4(e, +0x54, +0x56)
        ship_reaction(c, e, 14, E.order_rec(), 0x13);
    }
    if (E.order_until() < now && E.cls() == 1) {  // an Interceptor goes back to the arrest [BYTES seg13:0xacf4-0xad36]
        E.order(0x16);
        E.one_shot(2);
        E.replan_at(now);
        E.order_until(now + 5000u);
        return;
    }
    if (c.obj(kPoolShips, t).state != 0 || ship_or_zero(c, t, "seg13:0xad47").bec == 0) {  // [BYTES seg13:0xad38-0xad51]
        leave_gate0(c, e);
        return;
    }
    c.host.target(e, E.order_pool(), E.order_rec());  // [BYTES seg13:0xad82-0xadac]
    E.est_pool(14);
    E.est_rec(static_cast<std::int16_t>(t));
    E.est_app(2);
    E.est_act(1);
}

}  // namespace is::world
