// owner: world set-up (the single writer of this file in later stages).
// The wave set-up (docs/re/world.md "Wave set-up"): the load seg9:0x22b4 with the generator
// seg9:0x3202, the pool counts and the record templates seg10:0xb94; the briefing entry's part
// seg9:0x5cca with the icon fill seg9:0xb2a / seg9:0x0 and the placement seg9:0x6624 (rocks,
// pool 9, pool 4, gates and hazards); and the ships at the wave start seg9:0x60bc. Portable: the
// filesystem through nav::Walker, the icons through IconReader, rand() through shell::Rng.
//
// Call order of the original (world.md "Order"), which the hooks (hooks.cpp) follow:
//   state change to 1: srand (seg2:0x1b6b, the shell's play_area hook), state1_load()
//   (seg10:0x0 -> seg9:0x22b4), then new_game() when no game was restored (seg9:0xc06) and
//   enter_briefing() (seg9:0x5cca); at the briefing's end start_wave() (seg9:0x60bc).
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "formats/icon_source.h"
#include "nav/fs.h"
#include "shell/rng.h"
#include "world/world.h"

namespace is::world {

// What SHELL.ExtractIcon gives the original for a program file (ADR 0012 decision 5): the
// port's own reader. count() is ExtractIcon(path, -1), cell() ExtractIcon(path, k) drawn over
// black (nullopt where ExtractIcon returns no icon).
class IconReader {
public:
    virtual ~IconReader() = default;
    virtual int count(const std::string& dos_path) = 0;
    virtual std::optional<fmt::IconCell> cell(const std::string& dos_path, int index) = 0;
};

// What the set-up needs from the program.
struct SetupHost {
    SetupHost(shell::Rng& r, nav::Walker& w, IconReader& i) : rng(r), walker(w), icons(i) {}
    shell::Rng& rng;          // INSPACE's rand() (DGROUP:0x144c)
    nav::Walker& walker;      // the find calls of seg9:0x0, logged as nav.md's table
    IconReader& icons;        // the ExtractIcon calls of seg9:0x156, logged through the walker
    std::function<std::uint32_t()> now;          // GetCurrentTime (0 when unset)
    std::function<void(const std::string&)> log;  // the set-up's log lines
    // seg14:0x3eda, seg14:0x0(-1) and seg14:0x4074 at the placement (the ship records from
    // MASTER.SHP and the opponents). Unset: the player's ship alone (ship_setup_minimal).
    std::function<void(World&)> ship_setup;
    // The design type word DGROUP:0x970a + 0xad·design (ship word +0x4e), read by seg9:0x2828
    // [BYTES seg9:0x2855 seg9:0x285b seg9:0x29e3]. Unset: 0 for every design (the designs are M5).
    std::function<std::int16_t(int design)> design_type;
    // seg8:0x0, the view recentred on the player at the wave start (flight role).
    std::function<void(World&)> recentre;
};

// The DGROUP words the set-up reads before anything writes them: DGROUP:0x986 ... 0x990 from the
// initialised data (1, 1, 1, 1, 1, 2) and the uninitialised DGROUP:0x9346, 0x9432 and 0x992 (0)
// [BYTES DGROUP:0x986 DGROUP:0x990].
void power_on(World& w);

// ---- difficulty inputs
// seg6:0x11ec: the budget DGROUP:0x932a and DGROUP:0x9000 from `Complex` (w.complex) and the
// easy first waves [BYTES seg6:0x1204-0x1256 seg6:0x1258-0x1284].
void read_budget(World& w);
// seg9:0x484c: DGROUP:0x9334 = the number of score thresholds DGROUP:0x9bc + 4k (k < 12) below
// the rating dword DGROUP:0x5078 [BYTES seg9:0x484c].
void level_from_score(World& w);
// seg9:0xc06's resets of a new game (docs/re/world.md "New-game reset"): DGROUP:0x992 always;
// with DGROUP:0x92e4 = 0 the score, the rating, the wave, DGROUP:0x9346 and DGROUP:0x4eec = -1,
// DGROUP:0x9432 = 100, DGROUP:0xa1cf and the bytes DGROUP:0xa193 ... 0xa196 = 1; then the wave
// kinds DGROUP:0x986 ... 0x990 [BYTES seg9:0xc06 seg9:0xc56 seg9:0xc7f seg9:0xcc8].
void new_game(World& w);

// ---- the load seg9:0x22b4
// seg9:0x2828 as the load calls it at wave 0 (docs/re/world.md "seg9:0x2828", "DGROUP:0x9346"):
// DGROUP:0x9346 from a duel opponent's record, DGROUP:0x9332 from DGROUP:0x9432, and the
// DGROUP:0x932c chance (cleared again at wave 0) [BYTES seg9:0x2828 seg9:0x2a09 seg9:0x2b4d].
void wave0_followups(World& w, SetupHost& h);
// seg9:0x3202, the wave generator: wave kind, target, category counts, pool-4 count, gate count,
// pool-2 units and the opponents [BYTES seg9:0x3202] [OPEN: world.md "Generator draw loop";
// the port follows the routine's 61 rand() sites in address order, see setup.cpp].
void generate(World& w, shell::Rng& rng);
// seg9:0x22b4 without its drawing: roles, seg9:0x2828 for wave 0, the generator, DGROUP:0x9432,
// the pool counts, the reallocation of pools 0-14 and the templates [BYTES seg9:0x22b4
// seg9:0x24b5-0x2510 seg9:0x2513-0x257e seg9:0x2590 seg9:0x25a3].
void load(World& w, SetupHost& h);
// seg10:0xb94: the per-pool constants [BYTES seg10:0xb94].
void apply_templates(World& w);

// ---- programs and icons
// seg9:0xb2a: resets pools 0 and 1 and runs seg9:0x0(8), the enumeration of DGROUP:0x38c8 +
// "*.exe" then "*.dll" (attribute 1) with seg9:0x156 per file [BYTES seg9:0xb2a seg9:0x0
// seg9:0x156 seg9:0x94c seg9:0x9b0 seg9:0xac4 seg9:0x278].
void fill_icons(World& w, SetupHost& h);
// seg9:0x8da: a cell is blank when every pixel equals strip pixel (0, 0), black [BYTES seg9:0x8da].
bool blank_cell(const StripCell& c);

// ---- the placement seg9:0x6624 and its placers
void place(World& w, SetupHost& h);
void place_rock(World& w, shell::Rng& rng, int i);   // seg10:0x864
void place_pool9(World& w, shell::Rng& rng, int i);  // seg10:0xa26
void place_pool4(World& w, shell::Rng& rng, int i);  // seg9:0xdf6
void place_icon(World& w, shell::Rng& rng, int i);   // seg10:0x736
void place_gates(World& w, SetupHost& h);            // seg9:0x10d4 (gates and hazards)
// The centre exclusion of the four placers: ((1600 - W/2) << s, ...) in 16 bits [BYTES
// seg10:0x998 seg10:0x99d].
Rect16 centre_exclusion(const World& w);
// The player's ship alone (DGROUP:0x9316 = 0, pool 14 one record, heading +0x28 = 0) for
// set-ups without the ship set-up of seg14 [DESIGN] [OPEN: seg14:0x0 is M5].
void ship_setup_minimal(World& w);

// The raw cell of a ship's sheet as seg14:0x11a8 writes it to the ship's object +0x22 and +0x24
// before seg14:0x0 scales it [BYTES seg14:0x12d8-0x12f0 seg14:0x1337 seg14:0x134d]: the side is
// ISPACE.DLL ordinal 5 (seg10:0x86ca) after the sheet's render, the word +8 (biHeight) of the
// body DIB that ISPACE.DLL seg6:0x26a stores at seg6:0x2ae in ISPACE.DLL DGROUP:0x1c72 [BYTES
// ISPACE.DLL seg6:0x266 seg6:0x26a seg6:0x2ae seg6:0xba]; the gap is 5 for an odd side, else 6
// [BYTES seg14:0x12e0 seg14:0x12e4 seg14:0x12eb]. The writes happen only for a ship without a
// roster record (+0xd6 = -1) whose image slot 0x28 + i was empty [BYTES seg14:0x12f5
// seg14:0x1322].
struct ShipCellRaw {
    std::int16_t side = 0, gap = 0;
};
ShipCellRaw ship_cell_raw(std::int16_t body_height);
// seg14:0x0's size step for one ship [BYTES seg14:0xb97-0xc30]: at Size 0 an odd side is raised
// by 1 with gap 4; then +0x22 = (side << Size) >> 1, +0x24 = (gap << Size) >> 1 and the extent
// +0x20 = side << 3 (16-bit). The design words DGROUP:0x96fc, 0x96d2, 0x96d4, 0x96d6 + 0xad·n
// and the frame count +0x5b (from the sheet's GetObject at seg14:0xd59) are not written here
// [OPEN: the sheet is the sprite track's].
void ship_cell_sizes(ObjectRecord& o, ShipCellRaw raw, std::int16_t size);

// ---- the entries
// seg10:0x0 -> seg9:0x22b4(0): the state-1 load before the briefing.
void state1_load(World& w, SetupHost& h);
// seg9:0x5cca's world part: the load again, pool 6, the icon fill, seg9:0x94ba, the counts, the
// placement between the wave increments, the pool-2 rewrite and the wave increment seg9:0x5fbf
// [BYTES seg9:0x5cca seg9:0x5d3c seg9:0x5d66 seg9:0x5e58 seg9:0x5e5d seg9:0x5e60 seg9:0x5e9c
// seg9:0x5ecd seg9:0x5fbf].
void enter_briefing(World& w, SetupHost& h);
// seg9:0x94ba, the icon overlays on the strip DGROUP:0x3a02 [BYTES seg9:0x94cd-0x954f]: for each
// icon k < 8, seg7:0x2512(k, byte - 1) when directory byte +0x1c8 + k is non-zero, icon `flame`
// byte +0x2f = 0x9c when it is 2 (ice), and in a kind-2 wave seg7:0x2512(k, 0) when DGROUP:0x9334
// = 0 or, with DGROUP:0x9334 != 0, when rand() % 3 = 0 (x.rng). The briefing entry seg9:0x5cca
// and the rescale's reload path seg10:0x9fb3 call it (M4 s8 integrate).
struct CollideCtx;
void icon_overlay_pass(CollideCtx& x);
// seg9:0x60bc's world part: the ships at the wave start (world.md "Ships at wave start").
void start_wave(World& w, SetupHost& h);
// The seed of the wave start: seg9:0x60bc -> seg11:0x3cc2 -> seg11:0xafe, srand(GetCurrentTime)
// [BYTES seg11:0xb22 seg11:0xb28].
inline void seed_wave(shell::Rng& rng, std::uint32_t tick) {
    rng.srand(static_cast<std::uint16_t>(tick & 0xffff));
}

// One line naming the pool counts (allocated/in use) and the generator's outputs.
std::string describe_counts(const World& w);

}  // namespace is::world
