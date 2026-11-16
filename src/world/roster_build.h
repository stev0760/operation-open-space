// The wave's ships: seg14:0x0(mode), the roster builder that picks every ship of a wave (the
// player, the known ships of a revisited directory, the teammates, the other teams, the
// Enforcers; at most 8) and places them, with its pickers seg14:0x2300 and seg14:0x3274, the
// class routine seg14:0x40d8, the record step seg14:0xdd6, the names seg9:0x5be0, the per-ship
// fill seg14:0x1430 and the relation memory seg14:0x421c. Portable, no Win32 (M5 stage 3 part B,
// track ROSTER).
//
// Sources: work/analysis/m5/s3-roster.md with s3-roster-verify.md (corrections 5, 33, 43, 52 and
// the Omissions, which win where the two differ), docs/re/ships.md, docs/re/pilots.md, and the
// listings read again for the routines part A left open (work/world/m5s3/roster-report.md,
// "Readings no verifier has checked"). The rand() draws follow the original's order (ruling R1):
// seg14:0x2300 and seg14:0x3274 each draw and discard one rand() at entry, seg14:0xdd6 two per
// new record, seg16:0x3784's computer shop (src/world/shop.h) its own in the tail loop.
//
// The player's part of seg14:0x0 stays roster.h's roster_placement; ship_place ends in roster.h
// ship_from_roster. The program side (the sheets, the image slots, the design fill's flight
// words, the muzzle tables) goes through RosterHost.
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "world/world.h"

namespace is::fmt {
struct ShipRecord;
struct MasterShp;
struct MasterBod;
}  // namespace is::fmt
namespace is::shell {
class Rng;
}  // namespace is::shell

namespace is::world {

// ---- the DGROUP words of the builder that World does not hold (one set per program, as DGROUP
// is) [DESIGN: world.h is another track's file; the integrator may move them into World].
struct RosterWords {
    // DGROUP:0x4ed8 + 2k: the MASTER.SHP index of the Enforcer of class k that seg14:0x0 placed
    // (k = 0 Terminator, 1 Interceptor, 2 and 3 Patrol), read by seg14:0x1430 for a ship with
    // +0xd6 = -1 through ship byte +0x5e and by seg4:0x7636 [BYTES seg14:0x7e2 seg14:0x869
    // seg14:0x86c seg14:0x89d seg14:0x1461-0x146a]. Zeroed at every seg14:0x0 entry [BYTES
    // seg14:0x7b-0x8e].
    std::array<std::int16_t, 4> enforcer_shp{};
    std::int16_t mode = -1;        // DGROUP:0x95b6, seg14:0x0's argument [BYTES seg14:0x65]
    std::int16_t first = 0;        // DGROUP:0x95b0, the tail loop's first ship [BYTES seg14:0x1c8 seg14:0xb7a seg14:0xdba]
    // DGROUP:0x964c, the ships a call-in built (seg13:0xb644), lives in World::pilot (pilot.h).
    // DGROUP:0x3824, the 8 x 8 team table of ISPACE.DLL #5600 135 (seg10:0x38e0), word 8a + b;
    // the program copies dialogs::session().table in before each placement [DESIGN].
    std::array<std::int16_t, 64> team_table{};
    // DGROUP:0x9336 + ... are World's (w9336, opponents, w932c, w9346, w992, w994, w4f1c).
};
RosterWords& roster_words();

// The four words of image slot 0x28 + j (structs.md "Image slots").
struct SlotWords {
    std::uint16_t dc = 0, bitmap = 0, old_bitmap = 0, palette = 0;
};

// ---- what seg14:0x0 needs from the program
class RosterHost {
public:
    virtual ~RosterHost() = default;
    virtual const fmt::MasterShp& ships() = 0;    // far DGROUP:0x6e20, DGROUP:0x95aa records
    virtual const fmt::MasterBod& bodies() = 0;   // far DGROUP:0x6e1c
    virtual shell::Rng& rng() = 0;                // seg5:0xdc
    // GetPrivateProfileString("Inner Space", "MainShip", "EAGLE", buf, 20) [BYTES seg14:0x23a8].
    virtual std::string main_ship() = 0;
    // WritePrivateProfileString(MainShip = "TRANSFORMER") [BYTES seg14:0x240a seg14:0x2420].
    virtual void main_ship_rewritten() {}
    virtual std::string pilot() = 0;              // DGROUP:0x380c (INI ThePlayer, 12 bytes)
    // seg13:0x218a(team, buf): INI [Teams] Team<team + 1> (seg9:0x5c00).
    virtual std::string team_name(int team) = 0;
    virtual void beep() {}                        // USER MessageBeep(0) [BYTES seg14:0xe69-0xe76]
    // DGROUP:0x95a8, set by the demo player's death (seg16:0xaea, effects.h dg_95a8); seg14:0x0
    // reads and clears it [BYTES seg14:0x10-0x4c].
    virtual std::int16_t demo_death() { return 0; }
    virtual void clear_demo_death() {}

    // ---- the image slots 0x28 + j: the DC DGROUP:0x39e8, the bitmap DGROUP:0x3ad0, the old
    // bitmap DGROUP:0x3974 and the palette DGROUP:0x3a5c + 2·(0x28 + j). seg14:0x0 saves the four
    // words of every old slot at its entry (seg14:0x1f9-0x223) and writes saved words into the new
    // slots (seg14:0x9a7-0x9df); the host keeps the words as its image table does [DESIGN].
    virtual SlotWords slot_words(int j) = 0;
    virtual void set_slot_words(int j, const SlotWords& v) = 0;
    // seg10:0x1eb6(j, ...): frees slot 0x28 + j (palette back, old object back, DeleteDC,
    // DeleteObject, words zeroed) [BYTES seg10:0x1eb6-0x1f2e].
    virtual void slot_free(int j) = 0;

    // seg14:0x1b84(i, n, R, s) inside seg14:0x1430(i): ship i's design n from SHP record s. The
    // port's portable part is design_fill_portable below; the program adds the flight words.
    virtual void design_fill(World& w, int i, int shp) = 0;
    // seg16:0x3784(i, 1) [BYTES seg14:0xd32].
    virtual void purchases(World& w, int i) = 0;
    // The rest of the tail loop's iteration after seg16:0x3784: GetObject on the sheet (+0x5b),
    // the cell sizes and design +0x2a/+0x00/+0x02/+0x04, seg10:0x5d3c(i), the restored wave's
    // seg8:0x2cd8, seg10:0x1460/0x1620/0x1502 [BYTES seg14:0xb86-0xcf6 seg14:0xd36-0xd9d].
    virtual void ship_tail(World& w, int i) = 0;
    // seg10:0x546e(k) for k = 0 ... 0x1b (mode -1 only) [BYTES seg14:0xda0-0xdb4].
    virtual void weapon_images() {}
    // seg10:0x754a(id, 0, 0, 5), the mode-5 preload of sound `id` (sound.md "Mode-5 preloads"),
    // from seg14:0x1b84 [BYTES seg14:0x1e2d seg14:0x1fb9].
    virtual void preload_sound(int /*id*/) {}
    virtual void todo(const std::string& what, const char* address) = 0;
};

// ---- the design words of the builder (rating.h DesignRating, World::design_ratings)
// The design team word DGROUP:0x970a + 0xad·n (design +0x38), 0 outside 0 ... 7 [DESIGN].
std::int16_t design_team(const World& w, int n);
void set_design_team(World& w, int n, std::int16_t team);
// seg9:0xdb4-0xdca, the GAME OVER entry seg9:0xd6c: designs 0 ... 9 get +0x38 = 0 and +0x4e
// (DGROUP:0x9720) = 0 [BYTES seg9:0xdb4-0xdca]. The port holds designs 0 ... 7.
void game_over_designs(World& w);
// seg14:0x1b84(i, n, R, s) for ship i of design n = +0x4e from SHP record `shp` and its body, all
// of it but the design name strings and the words nothing in the port reads (+0x18, +0x1a, +0x36,
// +0x64, +0x66, +0x70, +0x84, +0x93, +0xa4, +0xa5, +0xa9, +0xab): rating.h's design_rating into
// World::design_ratings[n] (keeping the team word seg14:0x14ca wrote, +0x4e = 1, the mass, the
// blast count, +0xa3's carry), the body bytes and D (design_words.h), the wing clear, the object's
// mass, hit sound, radar brush, damage 200 / 0, the three callbacks and +0x2e / +0x3c / +0x54,
// the flight words (flight.h apply_flight_design) and ship bytes +0xe5, +0xe6, +0x0a, +0xed,
// +0x5f and words +0xa0 / +0xa2 [BYTES seg14:0x1b93-0x22f2]. `now` is GetCurrentTime for the
// lock-out +0x84; `h` (optional) takes the two sound preloads.
void design_fill_portable(World& w, int i, const fmt::ShipRecord& shp, const fmt::MasterBod& bodies,
                          std::uint32_t now, RosterHost* h = nullptr);
// seg14:0x0's size step for ship i after its sheet exists (seg14:0xd6b-0xd9d, seg14:0xb86-0xc68):
// side and gap from roster bytes +0x7a / +0x7b (sign-extended) or, for +0xd6 = -1, from object
// +0x22 / +0x24; design +0x2a = side; at Size 0 an odd side + 1 with gap 4; object +0x22 =
// (side << Size) sar 1, +0x24 = (gap << Size) sar 1, +0x20 = side << 3; ship byte +0x5b =
// lo8((bm_height - +0x24) / (+0x22 + +0x24) - 1) with bm_height the sheet's GetObject biHeight;
// design +0x00 = +0x02 = trunc(side / 2); +0x04 < 0 -> 4 - +0x04 - side / 2. The program calls it
// from RosterHost::ship_tail [BYTES seg14:0xb86-0xc68 seg14:0xd6b-0xd9d].
void ship_tail_sizes(World& w, int i, std::int16_t bm_height);

// ---- the pickers
// seg14:0x40d8(v): design v's class (1 ... 10), or the raw sum outside the bands
// [BYTES seg14:0x40e7-0x421a]. A zero BOD +0x1b divisor gives 0 for that quotient
// [DESIGN ADR 0016].
std::int16_t design_class(const fmt::MasterShp& ships, const fmt::MasterBod& bodies, int v);
// seg14:0x31d0(flag, buf), the demo's random ship (DGROUP:0x9516 != 0): c = rand() % 20 + 10,
// then v = rand() % DGROUP:0x95aa until c has counted down to 0 (one step per draw while c > 0)
// and v is named, passes seg14:0x455c and is not team 7 (team 7 passes with flag != 0); buf = v's
// name, returns v [BYTES seg14:0x31de-0x3269]. seg14:0x2300 calls it with flag 0 (seg14:0x242f).
int demo_ship(const World& w, const fmt::MasterShp& ships, const fmt::MasterBod& bodies, shell::Rng& rng, int flag,
              std::string& buf);
// seg14:0x4520(r, last): the first ship j = 0 ... last whose +0xd6 = r, else -1
// [BYTES seg14:0x4520-0x455b].
int ship_with_roster(const World& w, int r, int last);
// seg14:0x2300(kind, slot): the best MASTER.SHP design for a new ship; never -1 (verify O11).
// Kind 0 is roster.h pick_main_ship's; kinds 1 (teammate), 2 (Patrol), 3 (Interceptor), 4 (other
// teams), 5 (Terminator). Sets World::w95b4.
int pick_design(World& w, RosterHost& h, int kind, int slot);
// seg14:0x3274(kind, slot): the best known roster record, -1 below -999; sets World::w95b2.
int pick_known(World& w, RosterHost& h, int kind, int slot);
// seg14:0xdd6(slot, shp, r0): a new record (DGROUP:0x95b4 > DGROUP:0x95b2) or the known r0;
// ship slot +0xd6 = it, DGROUP:0x992 += 40. Returns the record.
int roster_take(World& w, RosterHost& h, int slot, int shp, int r0);
// seg9:0x5be0(ship, r): roster record r's name +0x65 (the pilot for DGROUP:0x9646 or in Demon's
// Gate, else the team's name cut at ' ' or a final 'S', " " and the decimal r).
void roster_name(World& w, RosterHost& h, int ship, int r);
// seg14:0x421c(i): ship i's relation memory written back into its roster record at the wave end.
void relation_memory(World& w, int i);

// ---- the placement
// seg14:0x1430(i): the team word, seg14:0x1b84 (h.design_fill), then the per-ship fill (roster.h
// ship_from_roster with the no-roster path and the relation fill).
void ship_place(World& w, RosterHost& h, int i);

struct RosterBuild {
    std::array<std::int16_t, 8> fresh{};  // [bp-0x54 + 2k]: the slot's sheet is built anew
    int count = 0;                        // the slots filled ([bp-0xe] at seg14:0x8e4)
};
// seg14:0x0(mode): mode -1 at the wave set-up (seg9:0x688a), a call-in kind or a team from
// seg13:0xae68 (seg13:0xb45c). The caller brackets it with seg14:0x3eda / seg14:0x4074.
RosterBuild build_wave_roster(World& w, RosterHost& h, int mode);

}  // namespace is::world
