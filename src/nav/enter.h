// Enter of the Navigation window, seg6:0x301a (docs/re/nav.md "Enter", "Values handed to the
// wave set-up", "Race and duel boxes"; docs/re/shell.md "Game-start dialogs" item 12): the
// Demon's Gate, race and duel gates, the lock test, the acceptance probe of an ordinary
// directory, the values handed to the wave set-up, the INI writes of the boxes and the
// directory record. Portable: the filesystem through the Walker, the strings through Strings,
// the boxes as the CB_GETCURSEL values the window read (boxes.h read_boxes()).
//
// The window side is install_parts() (parts_win32.cpp), which plugs Enter, the boxes and the
// demo pilot into nav.h's Parts; enter() is the whole routine for the portable model and the
// tests, check() the part before the window's own acceptance (nav.h accept()).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "nav/boxes.h"
#include "nav/fs.h"
#include "nav/nav_rules.h"
#include "nav/tree.h"

namespace is::settings {
class IniFile;
}

namespace is::nav {

using HostStrings = std::function<std::string(int)>;

// The globals Enter writes for the wave set-up (nav.md "Values handed to the wave set-up")
// [BYTES seg6:0x3044 seg6:0x30e7 seg6:0x30ed seg6:0x3339 seg6:0x3392 seg6:0x33dc seg6:0x343e
// seg6:0x348d seg6:0x351d seg6:0x3569 seg6:0x35b3 seg6:0x3642].
struct Handover {
    std::string directory;  // DGROUP:0x38c8: the path, "DEMON'S GATE", the course or "Dodge City"
    int demons_gate = 0;    // DGROUP:0x92e8: 1 = Demon's Gate
    int race = 0;           // DGROUP:0x94e: 4 x course + racers code (0 = no race)
    int duel = 0;           // DGROUP:0x950: opponents code + 4 x arena (0 = no duel)
    int difficulty = 0;     // DGROUP:0x94a8: the race or duel difficulty index
    int opponent = 0;       // DGROUP:0x94aa: the opponent index, 2 under DEMO
    // DGROUP:0x9ea0: race laps 1, 5 or 10; Enter leaves it for another index. It lies past
    // DGROUP's initialised bytes (0 at load), and the wave set-up writes it as well (seg9:0x3ae5,
    // seg9:0x3afd), which the port does not model yet [OPEN].
    int laps = 0;
    bool operator==(const Handover& o) const {
        return directory == o.directory && demons_gate == o.demons_gate && race == o.race &&
               duel == o.duel && difficulty == o.difficulty && opponent == o.opponent &&
               laps == o.laps;
    }
};

// What persists between Enter calls.
struct EnterState {
    Handover handover;
    // DGROUP:0xe32: the demo's alternation flag; a race or duel Enter in the demo sets it, the
    // next Enter that leaves both DGROUP:0x94e and 0x950 at 0 clears it [BYTES seg6:0x3147
    // seg6:0x31a3 seg6:0x31cc-0x31da].
    int demo_toggle = 0;
};

// The DemoType values (INI DemoType, DGROUP:0x95ba) of the DEMO and PLAY command lines; the
// initial selection's table is tree.h initial_selection() [BYTES seg6:0x3c4c-0x3d8e].
constexpr int kDemoTypeDefault = 519, kDemoTypeRoot = 520, kDemoTypeRace = 521,
              kDemoTypeDuel = 522;

// What seg6:0x301a reads besides the state.
struct EnterInputs {
    bool armed = true;         // DGROUP:0xd20 (seg6:0x1c72's argument)
    int state = 0;             // DGROUP:0x90f4
    Game game;                 // DGROUP:0x920c and DGROUP:0x95bc for seg6:0x3f4c
    bool command_line = false; // DGROUP:0x90f8: a DEMO or PLAY command line
    bool demo = false;         // DGROUP:0x8e5e: the DEMO command line's flag
    int demo_type = kDemoTypeDefault;  // DGROUP:0x95ba
    int wave = 0;              // DGROUP:0x91f8
    int restored = 0;          // DGROUP:0x92e4
    BoxValues boxes;           // CB_GETCURSEL of the seven combos
};

struct EnterResult {
    bool accepted = false;     // seg6:0x301a returned 1
    EnterAction action = EnterAction::kRefused;
    std::string dir;           // DGROUP:0x6990 afterwards (the course name for a race)
    // WritePrivateProfileString of the boxes in the original's order (nav.md "Race and duel
    // boxes"), each value seg6:0xf1a's decimal text.
    std::vector<std::pair<std::string, int>> ini_writes;
    int record = -1;           // DGROUP:0x94a2 after seg6:0x36b6, -1 when no record was added
    bool added = false;        // DGROUP:0x94b0
    Handover handover;         // the state's values when Enter returned
};

// LoadString(id, buf, 0x28) as Enter loads its names [BYTES seg6:0x3090 seg6:0x312a seg6:0x3186].
constexpr int kEnterStringMax = 0x28;

// seg6:0x301a with DGROUP:0x6990 = dir (updated in place: the course, "Dodge City" or
// "DEMON'S GATE" replace it). `ini` gives the DEMO keys DRacers, DDiff, DLaps and DuelDiff and
// takes the box writes (none when null); `rand` is seg5:0xdc, called only for a DEMO race with
// DLaps = 0. The acceptance probe seg6:0x4190 goes through the Walker as
// "seg6:0x301a via seg6:0x4190", the row of nav.md "Filesystem calls"
// [BYTES seg6:0x301a seg6:0x3029-0x303a seg6:0x3044 seg6:0x3049-0x30ed seg6:0x3147-0x321c
// seg6:0x3208-0x348d seg6:0x34b5-0x3642 seg6:0x3661-0x3683 seg6:0x3686 seg6:0x36b6].
EnterResult enter(EnterState& state, std::string& dir, const EnterInputs& in, Walker& walker,
                  Strings& strings, Records& records, settings::IniFile* ini,
                  const std::function<int()>& rand);
// seg6:0x301a up to the acceptance seg6:0x3686: enter() without the directory record
// (record -1, added false), for a window whose own acceptance adds it (nav.cpp accept()).
EnterResult check(EnterState& state, std::string& dir, const EnterInputs& in, Walker& walker,
                  Strings& strings, const Records& records, settings::IniFile* ini,
                  const std::function<int()>& rand);

// The wave set-up's decoding of the hand-over (nav.md "Decoding"): kind 6 race or 7 duel
// (DGROUP:0x9320), DGROUP:0x9338 the opponents code ((code - 1) mod 4) and DGROUP:0x9322 the
// course ((DGROUP:0x94e - 1) / 4), both by signed idiv; the duel wins when both are set
// [BYTES seg9:0x3db6 seg9:0x3dbd seg9:0x3dd7 seg9:0x3de6 seg9:0x3e00 seg9:0x3e10].
struct WaveKind {
    int kind = 0;       // DGROUP:0x9320: 6 race, 7 duel, 0 neither (left as it was)
    int opponents = 0;  // DGROUP:0x9338
    int course = 0;     // DGROUP:0x9322 (race only)
};
WaveKind decode(const Handover& h);

// What the parts need from the program beyond nav.h's Host (src/dialogs/hooks.cpp fills it).
struct EnterHost {
    HostStrings lang;                   // ENG_LANG.DLL, cp1252 (the box strings)
    settings::IniFile* ini = nullptr;   // INSPACE.INI: the box keys and the DEMO keys
    std::function<void()> ini_changed;  // after Enter wrote the box keys
    std::function<void(const std::string&)> log;
    std::function<int()> command_line;  // DGROUP:0x90f8
    std::function<int()> demo;          // DGROUP:0x8e5e
    std::function<int()> demo_type;     // DGROUP:0x95ba (INI DemoType)
    std::function<int()> rand;          // seg5:0xdc, the game's rand()
    std::function<std::uint32_t()> now;   // GetCurrentTime()
    std::function<std::uint32_t()> tick;  // DGROUP:0x1e50
    // The accepted Enter's values, before the acceptance: DGROUP:0x38c8, 0x92e8, 0x94e, 0x950,
    // 0x94a8, 0x94aa and 0x9ea0 for the wave set-up and the briefing.
    std::function<void(const Handover&)> handover;
};

#ifdef _WIN32
// Plugs the boxes, Enter and the demo pilot into the window (nav.h set_parts and
// set_pilot_target), in src/nav/parts_win32.cpp:
//   - Parts::create_boxes, layout_boxes and show_box: boxes.h create_boxes, place_boxes and
//     show_boxes;
//   - Parts::enter: check() on nav::directory() with read_boxes(), the state kept here, then
//     EnterHost::handover and nav::accept(handover.directory);
//   - Parts::initial: DGROUP:0x94e and DGROUP:0x950 from the initial selection (0 unless a
//     DEMO or PLAY command line set them), so every fill clears them as seg6:0x37ca does;
//   - Parts::demo_pilot(1): pilot_open(now, tick); the state-0 frame calls pilot_frame(tick).
void install_parts(const EnterHost& host);
// The state Enter keeps (DGROUP:0xe32 and the hand-over globals).
EnterState& enter_state();
#endif

}  // namespace is::nav
