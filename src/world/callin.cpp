// Mid-wave arrivals (callin.h): seg13:0xadbe and seg13:0xae68, ported from the listing
// [BYTES seg13:0xadbe-0xae67 seg13:0xae68-0xb6ef] with the original's 16-bit widths (ADR 0013
// decision 7) and its rand() order (ruling R1).
#include "world/callin.h"

#include "shell/rng.h"
#include "world/flight.h"
#include "world/player_sheet.h"
#include "world/roster.h"
#include "world/ship_entry.h"
#include "world/world.h"

namespace is::world {

namespace {

// The ship pool literal of seg13:0xae68: every pool push is `push 0xe` and every object read goes
// through the pool-14 pointer DGROUP:0x1e30 (pitfalls.md: a literal 14 is not DGROUP:0x9324)
// [BYTES seg13:0xb095 seg13:0xb170 seg13:0xb1a4 seg13:0xb5ca].
constexpr int kPool14 = 14;

bool ship_ok(const World& w, int i) {
    return i >= 0 && i < kShipCount && w.pools.records(kPool14) > i;
}
std::int8_t sb(const ShipRecord& s, int off) { return sbyte_at(s, off); }
void set_dword(Dword& d, std::uint32_t v) { d.set(v); }
void set_dword_at(ShipRecord& s, int off, std::uint32_t v) {
    set_word_at(s, off, static_cast<int>(v & 0xffffu));
    set_word_at(s, off + 2, static_cast<int>(v >> 16));
}
// The team word of ship k's design (DGROUP:0x970a + 0xad·(ship k +0x4e)).
std::int16_t team_of(const World& w, CallInHost& h, int k) {
    return h.team(w.ships[static_cast<std::size_t>(k)].design_index);
}

// rand() % DGROUP:0x9318 (`cdq; idiv word [0x9318]`, the remainder) [BYTES seg13:0xb0e5-0xb0ef].
// A zero gate count would fault the original's idiv; the port takes gate 0 [DESIGN ADR 0016].
std::int16_t gate_draw(World& w, CallInHost& h) {
    const int r = h.rng().rand();
    if (w.gates == 0) {
        static bool once = false;
        guard_todo(once, "seg13:0xae68's rand() % DGROUP:0x9318 with no gate", "seg13:0xb0eb");
        return 0;
    }
    return imod16(static_cast<std::int16_t>(r), w.gates);
}

// seg13:0xb0f2-0xb156 (and seg13:0xb34f-0xb3ac, seg13:0xb557-0xb5b1): the object at gate g's
// first post, pool-8 record 2g, moved right by W[g]/2 + post `size` - object `size`/2, W[g] the
// gate's word DGROUP:0x6682 + 0x1c·g; `size` (+0x22) is in pixels and added to world units as the
// original does (ships.md step 3) [BYTES seg13:0xb0fa-0xb156].
void place_at_gate(World& w, ObjectRecord& o, std::int16_t g) {
    const int post = 2 * g;
    if (post < 0 || w.pools.records(kPoolGatePosts) <= post || g >= kMaxGates) {
        static bool once = false;
        guard_todo(once, "seg13:0xae68's gate post outside the pool-8 records", "seg13:0xb0f2");
        return;
    }
    const ObjectRecord& p = w.pools.rec(kPoolGatePosts, post);
    o.y = p.y;
    o.x = callin_gate_x(p.x, p.size, o.size, w.gate[static_cast<std::size_t>(g)].width);
}

// The goal and order words of an entered ship [BYTES seg13:0xb1a0-0xb1e3 seg13:0xb5fd-0xb640]:
// +0x52 = the activity, +0x54 = 14, +0x56 = the requester, +0x32 = 14, +0x34 = the requester,
// dword +0x94 = now, byte +0xed = 1, +0x58 = `by`, dword +0x98 = now + 5000 (the off-world path;
// the new ship's +0x98 is written at the end), +0x38 = 3, byte +0xcd = the requester's low byte.
void entered_fields(ShipRecord& s, std::int16_t activity, int requester, std::int16_t by,
                    std::uint32_t now, bool order_until) {
    s.w52 = activity;
    s.w54 = kPool14;
    s.w56 = static_cast<std::int16_t>(requester);
    s.w32 = kPool14;
    s.w34 = static_cast<std::int16_t>(requester);
    set_dword(s.d94, now);
    set_byte_at(s, 0xed, 1);
    s.w58 = by;
    if (order_until) set_dword_at(s, 0x98, now + 5000u);
    s.w38 = 3;
    set_byte_at(s, 0xcd, requester & 0xff);
}

}  // namespace

Point16 CallInHost::screen_to_world(std::int16_t sx, std::int16_t sy) {
    todo("seg8:0x2bbe, Demon's Gate's screen point to the world (M6)", "seg8:0x2bbe");
    return Point16{sx, sy};
}

CallInWords& callin_words() {
    static CallInWords w;
    return w;
}
std::int16_t& CallInHost::called_in() { return callin_words().called_in; }
void CallInHost::set_callin_frame(std::uint32_t frame) { callin_words().callin_frame = frame; }

CallInRequest callin_request(const World& w, CallInHost& h, int ship, int kind,
                             const std::int16_t* team_table, std::int16_t dg_a163) {
    CallInRequest q;
    q.refused = true;
    // [BYTES seg13:0xae7c-0xae95]: a race or duel lets only kind 0x15 with byte DGROUP:0xa163 = 8.
    if ((w.kind == 6 || w.kind == 7) && (kind != kCallInInterceptor || (dg_a163 & 0xff) != 8)) return q;
    // [BYTES seg13:0xae9c-0xaeac]: 2·Detail + 1 >= DGROUP:0x964c, or kind 0x15.
    if (add16(add16(w.detail, w.detail), 1) < h.called_in() && kind != kCallInInterceptor) return q;
    // [BYTES seg13:0xaeae-0xaeb8]
    if (add16(w.detail, 3) < w.last_ship) return q;
    // [BYTES seg13:0xaeba-0xaec5]
    if (w.handover.demons_gate != 0 && kind != kCallInDemon) return q;
    if (!ship_ok(w, ship)) {
        static bool once = false;
        guard_todo(once, "seg13:0xae68's requester outside the ship records", "seg13:0xaefb");
        return q;
    }
    const ShipRecord& S = w.ships[static_cast<std::size_t>(ship)];
    // A live ship j < DGROUP:0x9316 of team t already serving the requester (+0xcd = ship)
    // [BYTES seg13:0xaf71-0xafa8 seg13:0xb008-0xb03f].
    const auto served = [&](std::int16_t t) {
        for (int j = 0; j < w.last_ship && j < kShipCount; ++j) {
            const ShipRecord& J = w.ships[static_cast<std::size_t>(j)];
            if (J.bec == 0) continue;
            if (h.team(J.design_index) != t) continue;
            if (sb(J, 0xcd) == ship) return true;
        }
        return false;
    };
    switch (kind) {
    case kCallInInterceptor:  // [BYTES seg13:0xaecd-0xaed2]
        q = {false, 1, 7, 0x16};
        return q;
    case kCallInTerminator:  // [BYTES seg13:0xaee3-0xaee8]
        q = {false, 0, 7, 0x17};
        return q;
    case kCallInHelp: {  // [BYTES seg13:0xaefb-0xafe4]
        if (sb(S, 0xcd) >= 0) return q;
        q.cls = -1;
        q.team = 4;
        q.activity = 1;
        const std::int16_t P = h.team(S.design_index);
        if (P == 2) {
            // word DGROUP:0x3828 = DGROUP:0x3824 + 2·(8·0 + 2), relation[0][2] (`jle`, signed)
            const std::int16_t r02 = team_table ? team_table[2] : std::int16_t{0};
            if (r02 > 50) q.team = 0;
        }
        if (P == 3) q.team = 3;
        if (P == 6) return q;
        // word DGROUP:0x3824 + 2·((T << 3) + P) < 0 refuses [BYTES seg13:0xaf5b-0xaf6c]
        const int ix = (q.team << 3) + P;
        std::int16_t rel = 0;
        if (team_table && ix >= 0 && ix < 64) {
            rel = team_table[ix];
        } else if (team_table) {
            static bool once = false;
            guard_todo(once, "seg13:0xae68's team table index outside DGROUP:0x3824's 64 words", "seg13:0xaf67");
        }
        if (rel < 0) return q;
        if (served(q.team)) return q;
        // [BYTES seg13:0xafab-0xafe4]
        if (S.w38 == 1 && q.team == 4) {
            const std::uint8_t b = static_cast<std::uint8_t>(sb(S, 0x146));
            if (b == 3 || b == 0xd || b == 8 || b == 0xa) return q;
        }
        q.refused = false;
        return q;
    }
    case kCallInPirates:  // [BYTES seg13:0xafed-0xb03f]
        if (sb(S, 0xcd) >= 0) return q;
        q.cls = -1;
        q.team = 3;
        q.activity = 4;
        if (served(3)) return q;
        q.refused = false;
        return q;
    case kCallInPatrol:  // [BYTES seg13:0xb048-0xb04d]
        q = {false, 2, 7, 0x14};
        return q;
    case kCallInDemon: {  // [BYTES seg13:0xb05d-0xb070]: [bp-0xa] is not written (no scan reads it)
        q.refused = false;
        q.cls = 0;
        q.team = ship_ok(w, w.player) ? team_of(w, h, w.player) : std::int16_t{0};
        q.activity = 4;
        return q;
    }
    default: {
        // No caller passes another kind; the original reads [bp-0xa], [bp-0xe] and [bp-0xc]
        // unwritten [DESIGN ADR 0016].
        static bool once = false;
        guard_todo(once, "seg13:0xae68 with an unknown kind: its request words are unwritten", "seg13:0xb075");
        q = {false, 0, 0, 0};
        return q;
    }
    }
}

std::int16_t call_in_request(World& w, CallInHost& h, int ship, int kind, int msg,
                             const std::int16_t* team_table, std::int16_t dg_a163) {
    const CallInRequest q = callin_request(w, h, ship, kind, team_table, dg_a163);
    if (q.refused) return 0;  // [BYTES seg13:0xae97]
    const std::uint32_t now = h.now();  // DGROUP:0x1e50
    // ---- a waiting officer (team 7) [BYTES seg13:0xb075-0xb1ea]
    if (q.team == 7) {
        if (kind == kCallInDemon) {
            // Kind 0x13 for a player of team 7: the scan reads the class [bp-0xa], which kind 0x13
            // leaves unwritten; the port takes 0 [DESIGN ADR 0016] [BYTES seg13:0xb05d-0xb070
            // seg13:0xb0b9].
            static bool once = false;
            guard_todo(once, "seg13:0xae68 kind 0x13 for team 7 reads the unwritten class [bp-0xa]", "seg13:0xb0b9");
        }
        for (int k = 0; k <= w.last_ship && k < kShipCount; ++k) {
            if (!ship_ok(w, k)) break;
            ObjectRecord& o = w.pools.rec(kPool14, k);
            if (o.state == 0) continue;
            ShipRecord& K = w.ships[static_cast<std::size_t>(k)];
            if (team_of(w, h, k) != q.team) continue;
            const std::int16_t c = sb(K, 0x5e);
            if (c != q.cls && q.cls != -1) continue;
            if (sb(K, 0x5e) < 2) h.play(0x70);  // SIRENS [BYTES seg13:0xb0c4-0xb0d7]
            h.purchases(k);                      // seg16:0x3784(k, 1) [BYTES seg13:0xb0d8-0xb0e2]
            place_at_gate(w, o, gate_draw(w, h));
            o.state = 0;                          // [BYTES seg13:0xb160]
            h.sound(0x27, kPool14, k, 1);         // HYPER [BYTES seg13:0xb16c-0xb179]
            h.entered(k);                         // seg12:0x2556(14, k), seg7:0x13b2(2, 14, k)
            entered_fields(K, q.activity, ship, static_cast<std::int16_t>(k), now, true);
            return 1;  // [BYTES seg13:0xb1e7]
        }
    }
    // ---- a ship already in the wave [BYTES seg13:0xb1ed-0xb3ef]
    bool stale = false;  // [bp-0x12]
    int k = 0;           // [bp-6]
    for (; k <= w.last_ship; ++k) {
        if (!ship_ok(w, k)) {
            static bool once = false;
            guard_todo(once, "seg13:0xae68's scan past the ship records", "seg13:0xb1ff");
            k = w.last_ship + 1;
            break;
        }
        ObjectRecord& o = w.pools.rec(kPool14, k);
        ShipRecord& K = w.ships[static_cast<std::size_t>(k)];
        if (kind == kCallInHelp) {
            // An off-world, live ship of the requester's own team [BYTES seg13:0xb2f9-0xb3ef].
            if (K.bec == 0 || o.state == 0) continue;
            if (team_of(w, h, k) != team_of(w, h, ship)) continue;
            place_at_gate(w, o, gate_draw(w, h));
            o.state = 0;
            h.sound(0x27, kPool14, k, 1);
            h.entered(k);
            entered_fields(K, q.activity, ship, static_cast<std::int16_t>(k), now, true);
            return 1;
        }
        if (kind == kCallInDemon) continue;  // [BYTES seg13:0xb2fd]
        if (o.state != 0) continue;           // [BYTES seg13:0xb221-0xb226]
        if (team_of(w, h, k) != q.team) continue;
        const std::int16_t c = sb(K, 0x5e);
        if (c != q.cls && q.cls != -1) continue;
        if (K.bec == 0) {  // a dead match [BYTES seg13:0xb251-0xb256]
            stale = true;
            continue;
        }
        const std::uint8_t cd = static_cast<std::uint8_t>(sb(K, 0xcd));
        if (cd != 0xff && sb(K, 0xcd) != ship) {  // it serves another requester [BYTES seg13:0xb258-0xb267]
            stale = true;
            continue;
        }
        if (kind == kCallInInterceptor || kind == kCallInTerminator) {
            // Already this requester's with an activity above 0x14 (`jg`) refuses [BYTES seg13:0xb275-0xb289].
            if (sb(K, 0xcd) == ship && K.w52 > 0x14) return 0;
            h.message(K.w56, kPool14, k, 0x3f);  // seg3:0x6ec(+0x56, 14, k, 0x3f) [BYTES seg13:0xb28d-0xb2a2]
        }
        // [BYTES seg13:0xb2a5-0xb2da]: no +0x32 / +0x34 / +0x98.
        K.w52 = q.activity;
        K.w54 = kPool14;
        K.w56 = static_cast<std::int16_t>(ship);
        set_byte_at(K, 0xed, 1);
        K.w58 = static_cast<std::int16_t>(k);
        set_dword(K.d94, now);
        K.w38 = 3;
        set_byte_at(K, 0xcd, ship & 0xff);
        if (kind == kCallInPirates) h.seg4_8346(k, kPool14, ship, 0xd);  // [BYTES seg13:0xb2de-0xb2f3]
        return 1;
    }
    if (stale) return 0;  // [BYTES seg13:0xb3f2-0xb3f6] (correction 52)
    // ---- a new ship [BYTES seg13:0xb3fa-0xb6e4]
    const std::int16_t by = static_cast<std::int16_t>(k);  // [bp-6] = the old DGROUP:0x9316 + 1
    h.designs_begin();                                     // seg14:0x3eda
    if (kind == kCallInInterceptor) {                      // [BYTES seg13:0xb3ff-0xb425]
        if (ship == w.player)
            h.play(0x70);
        else
            h.sound(0x70, kPool14, ship, 1);
    }
    if (msg != -1) h.message(ship, 7, 0, msg);  // seg3:0x6ec(ship, 7, 0, msg) [BYTES seg13:0xb428-0xb43d]
    h.seg11_238e();                             // [BYTES seg13:0xb440-0xb44b]
    h.build_roster(kind == kCallInHelp ? q.team : kind);  // seg14:0x0 [BYTES seg13:0xb44e-0xb461]
    w.w992 = add16(w.w992, 10);                 // [BYTES seg13:0xb462]
    h.designs_end();                            // seg14:0x4074
    const int n = w.last_ship;                  // DGROUP:0x9316 after seg14:0x0
    if (!ship_ok(w, n)) {
        static bool once = false;
        guard_todo(once, "seg13:0xae68's new ship outside the ship records", "seg13:0xb46c");
        return 2;
    }
    ObjectRecord& N = w.pools.rec(kPool14, n);
    ShipRecord& NS = w.ships[static_cast<std::size_t>(n)];
    N.src_y = N.gap;  // +0x12 = +0x24 [BYTES seg13:0xb46c-0xb47d]
    N.b2e = 0;
    N.owner_pool = -1;  // +0x32
    N.w36 = -1;
    const std::int16_t g = gate_draw(w, h);  // [BYTES seg13:0xb4b2-0xb4bc]
    if (w.handover.demons_gate != 0) {
        // Demon's Gate (M6): two more rand(), the distance rand() % (W / 5) + W / 5 and the heading
        // rand() % 16, from the player's screen point (object +0x00/+0x02) through seg8:0x2bbe;
        // the point goes to ship [bp-6], the old DGROUP:0x9316 + 1 [BYTES seg13:0xb4c8-0xb555].
        const int r1 = h.rng().rand();
        const std::int16_t w5 = idiv16(w.W, 5);
        std::int16_t dist = 0;
        if (w5 != 0) {
            dist = add16(imod16(static_cast<std::int16_t>(r1), w5), w5);
        } else {
            static bool once = false;
            guard_todo(once, "seg13:0xae68's Demon's Gate distance with DGROUP:0x90ea / 5 = 0", "seg13:0xb4df");
        }
        const int dir = imod16(static_cast<std::int16_t>(h.rng().rand()), 16);
        if (ship_ok(w, w.player)) {
            const ObjectRecord& P = w.pools.rec(kPool14, w.player);
            // imul word, then `cwd; and dx, 0xf; add ax, dx; sar ax, 4` on the low word
            // [BYTES seg13:0xb502-0xb52e].
            const std::int16_t sx = add16(idiv16(mul16(heading_dx(dir), dist), 16), P.scr_x);
            const std::int16_t sy = add16(idiv16(mul16(heading_dy(dir), dist), 16), P.scr_y);
            const Point16 p = h.screen_to_world(sx, sy);
            if (ship_ok(w, by)) {
                ObjectRecord& B = w.pools.rec(kPool14, by);
                B.x = p.x;
                B.y = p.y;
            } else {
                static bool once = false;
                guard_todo(once, "seg13:0xae68's Demon's Gate store to ship [bp-6] past the records", "seg13:0xb545");
            }
        }
        h.todo("seg13:0xae68's Demon's Gate arrival (M6)", "seg13:0xb4bf");
    } else {
        place_at_gate(w, N, g);  // [BYTES seg13:0xb557-0xb5b1]
    }
    N.state = 0;                    // [BYTES seg13:0xb5be]
    h.sound(0x27, kPool14, n, 1);   // [BYTES seg13:0xb5c4-0xb5d3]
    h.entered(n);                   // [BYTES seg13:0xb5d6-0xb5f1]
    entered_fields(NS, q.activity, ship, by, now, false);
    // The new ship's design word +0x42 (DGROUP:0x9714, captures icons) = 0 [BYTES seg13:0xb634-0xb63a].
    if (NS.design_index >= 0 && NS.design_index < kShipCount)
        w.design_ratings[static_cast<std::size_t>(NS.design_index)].captures = 0;
    h.called_in() = add16(h.called_in(), 1);  // DGROUP:0x964c [BYTES seg13:0xb644]
    if (kind == kCallInPirates || kind == kCallInHelp) {
        // [BYTES seg13:0xb64e-0xb6bb]: +0xed = 0, the message, word +0x62 + 2·requester = -300 / 300.
        set_byte_at(NS, 0xed, 0);
        h.message(ship, kPool14, n, kind == kCallInPirates ? 0x67 : 0x68);
        if (ship >= 0 && ship < 8)
            NS.r62[static_cast<std::size_t>(ship)] = kind == kCallInPirates ? std::int16_t{-300} : std::int16_t{300};
        if (kind == kCallInPirates) h.seg4_8346(n, kPool14, ship, 0xd);
    }
    set_dword_at(NS, 0x98, now + 5000u);              // [BYTES seg13:0xb6c1-0xb6d8]
    h.set_callin_frame(h.frames());  // DGROUP:0x8e2c = DGROUP:0x1e68 [BYTES seg13:0xb6dc-0xb6e0]
    return 2;
}

std::int16_t call_in(World& w, CallInHost& h, int ship, int kind, int msg, std::uint32_t* music_due,
                     const std::int16_t* team_table, std::int16_t dg_a163) {
    const std::int16_t r = call_in_request(w, h, ship, kind, msg, team_table, dg_a163);
    if (r == 0) return 0;  // [BYTES seg13:0xadd9-0xaddb]
    // [BYTES seg13:0xaddf-0xae41]: the switch subtracts on AL, so it sorts the low byte below 0x16.
    std::int16_t t = 2;
    std::int16_t ax = static_cast<std::int16_t>(kind);
    const auto k16 = static_cast<std::uint16_t>(kind);
    if (k16 == 0x16 || k16 > 0x16) {
        t = 2;
    } else if (kind == kCallInPirates || kind == kCallInDemon || kind == kCallInTerminator) {
        t = 7;
    } else if (kind == kCallInHelp) {
        ax = w.player;  // [BYTES seg13:0xade9]
        if (ship == w.player) {
            t = 8;
        } else if (ship_ok(w, w.player)) {
            const ShipRecord& P = w.ships[static_cast<std::size_t>(w.player)];
            if (P.w32 != 0xe) {
                t = -1;
            } else {
                ax = static_cast<std::int16_t>(ship);  // [BYTES seg13:0xae03]
                t = (P.w34 == ship && P.w38 == 1) ? std::int16_t{7} : std::int16_t{-1};
            }
        } else {
            t = -1;
        }
    }
    if (t == -1) return ax;  // [BYTES seg13:0xae43-0xae47]
    h.music(t, 0);           // seg10:0x7480(t, 0) [BYTES seg13:0xae49-0xae4e]
    const std::uint32_t due = h.now() + 12000u;  // [BYTES seg13:0xae53-0xae5d]
    if (music_due) *music_due = due;
    return static_cast<std::int16_t>(due & 0xffffu);
}

// ---- the program's call-in

CallInLinks& callin_links() {
    static CallInLinks l;
    return l;
}

namespace {

class LinkedCallInHost final : public CallInHost {
public:
    World* w = nullptr;
    shell::Rng& rng() override { return shell::game_rng(); }
    std::int16_t team(int design) override {
        if (callin_links().team) return callin_links().team(design);
        // DGROUP:0x970a + 0xad·n, World::design_ratings[n].team (rating.h) [DESIGN: the port holds
        // designs 0 ... 7].
        if (w && design >= 0 && design < kShipCount) return w->design_ratings[static_cast<std::size_t>(design)].team;
        return 0;
    }
    void play(int id) override {
        if (callin_links().play) return callin_links().play(id);
        todo("seg10:0x744c for the call-in", "seg10:0x744c");
    }
    void sound(int id, int pool, int rec, int mode) override {
        if (callin_links().sound) return callin_links().sound(id, pool, rec, mode);
        todo("seg10:0x754a for the call-in", "seg10:0x754a");
    }
    void music(int track, int flag) override {
        if (callin_links().music) return callin_links().music(track, flag);
        todo("seg10:0x7480 for the call-in", "seg10:0x7480");
    }
    void message(int recipient, int pool, int rec, int code) override {
        if (callin_links().message) return callin_links().message(recipient, pool, rec, code);
        todo("seg3:0x6ec for the call-in", "seg3:0x6ec");
    }
    void purchases(int ship) override {
        if (w) purchase_pass(*w, ship, 1);  // seg16:0x3784(k, 1) (roster.h)
    }
    void entered(int ship) override {
        if (sprite_links().ship_entered) return sprite_links().ship_entered(ship);
        todo("seg12:0x2556 and seg7:0x13b2 for the call-in without the drawing side", "seg7:0x13b2");
    }
    void build_roster(int mode) override {
        if (callin_links().build_roster) return callin_links().build_roster(mode);
        todo("seg14:0x0(mode) for the call-in unbound (track ROSTER)", "seg14:0x0");
    }
    void seg11_238e() override {
        if (callin_links().seg11_238e) return callin_links().seg11_238e();
        todo("seg11:0x238e(DGROUP:0xa7c, 0) for the call-in", "seg11:0x238e");
    }
    void seg4_8346(int ship, int pool, int rec, int code) override {
        if (callin_links().seg4_8346) return callin_links().seg4_8346(ship, pool, rec, code);
        todo("seg4:0x8346 for the call-in", "seg4:0x8346");
    }
    Point16 screen_to_world(std::int16_t sx, std::int16_t sy) override {
        if (callin_links().screen_to_world) return callin_links().screen_to_world(sx, sy);
        return CallInHost::screen_to_world(sx, sy);
    }
    std::uint32_t now() override { return callin_links().now ? callin_links().now() : 0u; }
    std::uint32_t frames() override { return callin_links().frames ? callin_links().frames() : 0u; }
    std::int16_t& called_in() override {
        if (callin_links().called_in)
            if (std::int16_t* p = callin_links().called_in()) return *p;
        return CallInHost::called_in();
    }
    void set_callin_frame(std::uint32_t frame) override {
        if (callin_links().callin_frame)
            if (std::uint32_t* p = callin_links().callin_frame()) {
                *p = frame;
                return;
            }
        CallInHost::set_callin_frame(frame);
    }
    void todo(const std::string& what, const char* address) override {
        if (callin_links().todo) callin_links().todo(what, address);
    }
};

}  // namespace

CallInHost& linked_callin_host(World& w) {
    static LinkedCallInHost h;
    h.w = &w;
    return h;
}

std::int16_t call_in_program(World& w, int ship, int kind, int msg) {
    CallInLinks& l = callin_links();
    std::int16_t* table = l.team_table ? l.team_table() : nullptr;
    const std::int16_t a163 = l.dg_a163 ? l.dg_a163() : std::int16_t{0};
    std::uint32_t* due = l.music_due ? l.music_due() : nullptr;
    return call_in(w, linked_callin_host(w), ship, kind, msg, due, table, a163);
}

}  // namespace is::world
