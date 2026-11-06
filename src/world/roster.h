// The ship between waves: the roster block DGROUP:0x95a4 (far DGROUP:0x6e24, 0x7c-byte records,
// records.h RosterRecord) and the routines that move a ship's state into it at the wave end and
// back out at the next placement. Portable, no Win32 (M4 s3 ROSTER).
//
// Sources: work/analysis/m4/s3-roster.md with the verdicts of s3-roster-verify.md (claims 5,
// 11, 20, 35 and 36 CORRECTED, 30 REFUTED, 31 limited to the wave-1 player call), docs/re/
// formats.md "Ship roster records", "Resuming"; ADR 0014 decisions 5 to 7. Oracle: core-0001's
// carry-over 39, 9, 39, 1 and core-0003's fuel = 40 x roster byte +0x1c (1209 -> 1200).
//
// The order of one placement seg9:0x6624 -> seg14:0x0(-1) for the player, as the port runs it
// (src/world/hooks.cpp ship_setup_player):
//   1. roster_placement: the fresh roster seg14:0x232-0x438 when DGROUP:0x91f8 <= 1, else the
//      later-wave choice seg14:0x43a-0x47e; then seg14:0x1430's SHP index with its clamp;
//   2. the design (seg14:0x1b84 inside seg14:0x1430: flight.h apply_flight_design, rating.h);
//   3. ship_from_roster: the rest of seg14:0x1430 (the roster -> ship fill);
//   4. purchase_pass(i, 1) when purchase_due (seg14:0xd1a-0xd32 -> seg16:0x3784(i, 1)).
// The wave end: roster_from_ship for each ship with a roster record (seg9:0x2b5d-0x2e59, also
// the save-time seg6:0x99d7-0x9cda), then roster_ratings after 500 frames (seg9:0x2e97).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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

// ---- logging: the stubs of the routines this file does not port report through here (the
// program binds app::log_todo; unset, nothing is logged) [DESIGN].
using RosterTodo = std::function<void(const std::string& what, const char* address)>;
void set_roster_todo(RosterTodo f);

// ---- the block

// DGROUP:0x931a, the highest roster index: roster.size() - 1, -1 for no block [DESIGN: the
// vector holds the count] [BYTES seg14:0x287 seg14:0xecd seg10:0x38d6 seg6:0x967f].
int roster_last(const World& w);
// Roster record r, or null for -1 or an index the block does not hold.
RosterRecord* roster_at(World& w, int r);
// The raw byte +off of a record (0 ... 0x7b), as the original's byte-indexed accesses read it
// (+0x3c + roster index, which reaches +0x3b for -1) [BYTES seg9:0x2b88 seg14:0x1a2c].
std::uint8_t& roster_byte(RosterRecord& r, int off);
// The 0x7c bytes of a record in the file's order, little-endian words (ADR 0014 decision 1),
// and back. `out` / `p` hold kRosterRecordSize bytes. The SAVE track's block is the records of
// w.roster in index order.
void roster_to_bytes(const RosterRecord& r, std::uint8_t* out);
RosterRecord roster_from_bytes(const std::uint8_t* p);

// The ship's credit dword +0xdc (words wdc, wde) [BYTES seg14:0x199e seg9:0x2ce8].
std::int32_t ship_credit(const ShipRecord& s);
void set_ship_credit(ShipRecord& s, std::int32_t v);

// ---- MainShip: seg14:0x455c and seg14:0x2300

// seg14:0x455c(v): 1 while DGROUP:0x11aa != 0, else 0 for a body whose MASTER.BOD word +0x21 is
// 178, 310, 320, 400, 440, 450, 520, 550, 560, 590, 600, 604, 605, 710, 720, 730, 750, 890, 900,
// 930 or 950 [BYTES seg14:0x4567 seg14:0x4589-0x45f0 seg14:0x45f4]. A record whose body index
// lies outside MASTER.BOD is accepted (the original reads past the table) [DESIGN].
bool ship_accepted(const World& w, const fmt::MasterShp& ships, const fmt::MasterBod& bodies, int v);

struct MainShipPick {
    int index = -1;            // the SHP index; -1 only for an empty MASTER.SHP
    bool rewrite = false;      // the validation pass wrote `MainShip=TRANSFORMER`
    std::string name;          // the buffer after the validation pass
};
// seg14:0x2300(0, 0), the wave-1 player call (claims 29 to 32): one rand() consumed; DGROUP:0x931c
// = 0 when above DGROUP:0x931a; the validation pass (DGROUP:0x9516 = 0) turns the buffer into
// "TRANSFORMER" for every record of its name that seg14:0x455c rejects; the selection takes the
// first accepted record whose name equals the buffer (best 20000). No match: DGROUP:0x95b4 =
// -1000 and index 1. With fewer than two records index 1 is outside the table, and the port
// keeps index 0 with a log line [DESIGN] (ADR 0014 decision 7). `main_ship` is INI `MainShip`
// as read with the original's buffer of 20 ("EAGLE" by default).
MainShipPick pick_main_ship(World& w, const fmt::MasterShp& ships, const fmt::MasterBod& bodies,
                            const std::string& main_ship, shell::Rng& rng);

// ---- the placement

// seg14:0x232-0x3f3's record for SHP record `shp` at index `index`: zero everywhere else
// (GMEM_ZEROINIT), +0x38 ... +0x3b = 100, +0x17 + a = 0xff, the second weapon rule (+0x1d,
// +0x1f, +0x29 with the DGROUP:0x2f8a remap), +0x72 ... +0x75 = 1, +0x1c = SHP +0x00, +0x02 =
// 15, +0x04 = 0, +0x64 = SHP +0x03 [BYTES seg14:0x2b0-0x3f3] (claims 1, 2).
RosterRecord fresh_record(const World& w, const fmt::ShipRecord& shp, int index);
// The remap of weapon `wpn` through word +0x36 of the weapon table (DGROUP:0x2f8a + 0x50·w,
// collide.h weapon_word) while DGROUP:0x11aa = 0 and the word is not -1 [BYTES seg14:0x33a-0x354
// seg14:0x16e5-0x170a]. Before the table is filled (its word 0 of block 0 reads 0, not the
// string id 186) the port leaves the weapon as it is and logs once [DESIGN].
std::uint8_t remap_weapon(const World& w, std::uint8_t wpn);

struct RosterPlacement {
    bool fresh_roster = false;  // the wave-1 path ran (DGROUP:0x91f8 <= 1)
    bool fresh_ship = true;     // seg14:0x0's local [bp-0x54] for slot 0 at the purchase test
    MainShipPick pick;          // the MainShip result (fresh roster only)
    int shp_index = -1;         // seg14:0x1430's SHP index s for the player, -1 for none
};
// seg14:0x0(-1)'s player part with DGROUP:0x91f8 = w.wave (the briefing has raised it), up to
// seg14:0x1430's SHP index [BYTES seg14:0x232-0x47e seg14:0x1457-0x148e]:
//   - DGROUP:0x91f8 <= 1 (or no roster block, which the original cannot reach [DESIGN]):
//     DGROUP:0x931c = DGROUP:0x9646 = 0, pick_main_ship, a block of one fresh record,
//     DGROUP:0x9338 = 0 in a player game outside kinds 6 and 7, the arrays DGROUP:0xa14d /
//     0xa157 zeroed, seg9:0x5be0 (roster_pilot_name), ship 0's +0xd6 = +0x4e = 0, DGROUP:0x1318 = 0;
//   - later waves: ship 0's +0x4e = the old player ship's, DGROUP:0x9646 = 0, +0xd6 =
//     DGROUP:0x931c; the fresh flag clears unless DGROUP:0x92e4 != 0, which also sets +0x4e = 0;
//   - then s = roster +0x00, set to 0 in the record when not below the SHP count.
// A DGROUP:0x931c outside the block reads record 0 with a log line [DESIGN].
// `pilot` is DGROUP:0x380c, the pilot name seg9:0x5be0 copies into the fresh record's +0x65.
RosterPlacement roster_placement(World& w, const fmt::MasterShp& ships, const fmt::MasterBod& bodies,
                                 const std::string& main_ship, shell::Rng& rng,
                                 const std::string& pilot = std::string());

// seg9:0x5be0(ship, r) for the player's ship (ship = DGROUP:0x9646) or in Demon's Gate
// (DGROUP:0x92e8 != 0): lstrcpy(roster record r + 0x65, DGROUP:0x380c), the pilot name
// (formats.md "Names" seg17:0x8e60) [BYTES seg9:0x5bee-0x5bfd seg9:0x5ca6-0x5cbb]. DGROUP:0x380c
// holds at most 11 characters (INI ThePlayer, 12 bytes, shell.md "Player name"); the port stops
// at the field's 13th byte [DESIGN]. Another ship's name (seg9:0x5c00-0x5ca4, its team's name
// cut at " S" plus its index) is M5 (M4 s4 fixer B: save.md DIFFERENT 8).
void roster_pilot_name(RosterRecord& r, const std::string& pilot);

// seg14:0x1430(i) after its seg14:0x1b84 (the design): the per-placement resets and, for a ship
// with a roster record, the roster -> ship fill: weapons (remapped, written back), the systems
// sign-extended from +0x38 ... +0x3b, fuel = 40 x +0x1c (unsigned `mul`, raised to 2/3 of the
// tank for a ship other than DGROUP:0x9646, capped at the tank with +0x1c rewritten, no refill),
// the wings, +0x24, +0x0a, credit, resources, +0x82, the wave counters +0x77 ... +0x79 and ship
// word +0x00, the relation words +0x62 + 2j. Without a record, seg14:0x1a5d-0x1b78 (computer
// ships, M5). Writes no +0x18, +0xe0 or +0xe1 (claim 9). `shp` is the record seg14:0x1430's s
// names. [BYTES seg14:0x14ad-0x1b78]
void ship_from_roster(World& w, int i, const fmt::ShipRecord& shp);

// seg14:0xd1a-0xd32: seg16:0x3784(i, 1) runs when the slot's fresh flag is set, DGROUP:0x91f8 =
// 1 or seg14:0x0's argument is not -1 [BYTES seg14:0xd1a seg14:0xd20 seg14:0xd27 seg14:0xd32].
bool purchase_due(const World& w, bool fresh, int arg = -1);
// Whether seg16:0x3468(i, 3), which seg16:0x3784(i, 1) always calls, rebuilds ship i's sheet
// with seg14:0x11a8(i): only when the placement ran seg16:0x3784 (World::sheet_due), the
// new-body flag DGROUP:0x9563 is 0 (else the body switch, M8), and not when the design's
// special-ability word +0x28 is -1 while the slot DGROUP:0x3a38 + 2i already holds a DC. A
// later wave without the local flag keeps the old sheet (world.md Ship sheets, Path; C067)
// [BYTES seg14:0xd1a seg14:0xd32 seg16:0x38d2 seg16:0x3477 seg16:0x348e seg16:0x34a0
// seg16:0x3547 seg16:0x362b] (M4 s4 fixer).
bool placement_rebuilds_sheet(bool sheet_due, std::uint8_t new_body, std::int16_t special, bool slot_has_dc);

// seg16:0x3784(i, mode), the VISIT track's purchases (src/world/purchases.*): the program binds
// it here (the fixer); unbound, purchase_pass runs the mode-1 fallback below [DESIGN].
using PurchasePass = std::function<void(World& w, int ship, int mode)>;
void set_purchase_pass(PurchasePass f);
void purchase_pass(World& w, int i, int mode);
// The mode-1 effects of seg16:0x3784 that the placement needs, for the unbound case: seg16:0x3468
// (i, k) for k = 0, 1, 2 when byte DGROUP:0x953e != 0 or DGROUP:0x92e4 = 0, seg16:0x3468(i, 3),
// seg16:0x3e80(i), seg13:0x33d6(i), ship +0x02 = 0 and object word +0x26 = 0; the credit is
// unchanged (claims 11-14 as corrected). seg16:0x3b18, the flag-driven calls, the weapon loop,
// seg16:0x3468's lamp-row early return and its wing rebuild are the VISIT track's [OPEN].
void purchase_pass_fallback(World& w, int i);

// seg9:0x6db0: every ship 0 ... DGROUP:0x9316 refuelled to its design tank. Only kind-6 waves
// call it, at the placement and at the debriefing entry [BYTES seg9:0x6901 seg9:0x6906
// seg9:0x6cee seg9:0x6cf3 seg9:0x6dc5 seg9:0x6dd4] (claim 25).
void refuel_all(World& w);

// ---- the wave end

// The class local [bp-0x10] of one update loop: it keeps its value from ship to ship, and the
// uninitialised start is 0 (ADR 0014 decision 5).
struct RosterUpdate {
    std::int16_t cls = 0;
};
// seg9:0x2b5d-0x2e59 for ship i (roster index R = +0xd6, which must not be -1): the ship ->
// roster write-back. The save-time body seg6:0x99d7-0x9cda is the same in effect; the listings
// differ in three places only (claim 20 as corrected): (a) the credit cap compare, seg9's
// `cmp eax, 0x4fff6; jle` against seg6's `cmp dx, 4; jl; jg; cmp ax, 0xfff6; jbe`, the same
// signed 32-bit test; (b) the credit pushed to seg5:0x314 as one dword (seg9:0x2d21) or as two
// words (seg6); (c) the loop exit, seg9:0x2da4 `jge` against seg6:0x9c24 `jl; jmp`. One routine
// serves both callers [DESIGN]. Writes +0x3c + ship[j].+0xd6, +0x38 ... +0x3b (+0x38 = 0 for a
// dead ship), slots 0 ... +0xcb - 1, +0x1d, +0x06, +0x1c = fuel / 40 (every time), +0x74, +0x75,
// +0x72, +0x73, +0x04, the credit (capped at 0x4fff6 in the ship too) / 10, the class +0x76 and
// the current directory record's list +0x1c0. No captured or destroyed counts (claim 19).
void roster_from_ship(World& w, int i, RosterUpdate& u);
// seg6:0x99b6's loop for the save (SAVE track): roster_from_ship for every ship 0 ... DGROUP:0x9316
// with a record, no flame-off, then DGROUP:0x931c = the player's +0xd6; nothing when
// DGROUP:0x91f8 <= 0 [BYTES seg6:0x99c5 seg6:0x99ca seg6:0x9d03] (claim 21).
void roster_from_ships_for_save(World& w);
// The class bands of seg9:0x2d49-0x2d5e: the first k = 1 ... 10 with DGROUP:0x952 + 2k <= v <
// DGROUP:0x96c + 2k (one table: 0x96c = 0x952 + 2·13), or `prev` when none matches
// [BYTES DGROUP:0x952 DGROUP:0x96c] (claim 22).
std::int16_t roster_class(std::int16_t v, std::int16_t prev);
// seg9:0x2e97-0x2f74, after the ship loop when the frame dword DGROUP:0x1e68 > 500 (the caller
// tests it): word +0x06 of every record 0 ... DGROUP:0x931a moves by the class rule against
// DGROUP:0x9334 and by |SHP +0x05 - DGROUP:0x9332|, clamped at 0 on the 16-bit sum
// (claims 23, 24). An SHP index outside `ships` reads byte 0 [DESIGN].
void roster_ratings(World& w, const fmt::MasterShp& ships);

// ---- M5: ships joining the roster

// seg14:0xdd6(slot, shp, r0) (claim 36 as corrected, claim 37): with DGROUP:0x95b4 above
// DGROUP:0x95b2 a record is added while DGROUP:0x931a <= 38, else the record 1 ... 39 with the
// lowest signed +0x06 below 1000 that ships 0 ... slot - 1 do not hold is reused (none: a beep
// and record 10), its index cleared from every directory list +0x1c0; the record is then
// initialised (+0x06 = 15, +0x02 and +0x04 from rand()). Otherwise r0 is returned and the
// known-ship path seg14:0x10d0 is [OPEN M5]. `beep` stands for MessageBeep(0).
int roster_add(World& w, int slot, const fmt::ShipRecord& shp, int shp_index, int r0, shell::Rng& rng,
               const std::function<void()>& beep = {});

}  // namespace is::world
