// Enter of the Navigation window, seg6:0x301a (see enter.h).
#include "nav/enter.h"

#include "settings/settings.h"

namespace is::nav {

namespace {

// The path with byte 1 (the ':' of "D:\...") set to ' ', as Enter hands it to seg6:0x3f4c for
// the built-in rows, so no directory record can match it [BYTES seg6:0x30c5 seg6:0x322b].
std::string blank_colon(std::string p) {
    if (p.size() > 1) p[1] = ' ';
    return p;
}

// local_172[lstrlen(local_172) - lstrlen(suffix)] = 0: the path without its last character
// and the suffix [BYTES seg6:0x32ed seg6:0x35f2]. A suffix longer than the path would write
// before the buffer in the original; the port gives the empty string [DESIGN].
std::string cut_suffix(const std::string& s, const std::string& suffix) {
    return suffix.size() <= s.size() ? s.substr(0, s.size() - suffix.size()) : std::string();
}

int ini_int(const settings::IniFile* ini, const char* key, int def) {
    return ini ? ini->get_int(settings::kGameSection, key, def) : def;
}

}  // namespace

EnterResult check(EnterState& st, std::string& dir, const EnterInputs& in, Walker& w, Strings& s,
                  const Records& records, settings::IniFile* ini, const std::function<int()>& rand) {
    EnterResult res;
    Handover& h = st.handover;
    // Returns 0 at once while DGROUP:0xd20 = 0 or in state 1 [BYTES seg6:0x3029-0x303a].
    if (!in.armed || in.state == 1) {
        res.handover = h;
        res.dir = dir;
        return res;
    }
    auto refuse = [&] {
        res.handover = h;
        res.dir = dir;
        return res;
    };
    auto write = [&](const char* key, int value) {
        // seg6:0xf1a's decimal text and WritePrivateProfileString [BYTES seg6:0xf1a].
        res.ini_writes.emplace_back(key, value);
        if (ini) ini->set_int(settings::kGameSection, key, value);
    };

    h.directory = dir;  // lstrcpy(DGROUP:0x38c8, DGROUP:0x6990) [BYTES seg6:0x3044]
    h.demons_gate = 0;
    // The path without its last character (the trailing '\'), in two copies: one for the
    // ends-with tests, local_172 for the course names [BYTES seg6:0x3049-0x306b].
    const std::string cut = dir.empty() ? dir : dir.substr(0, dir.size() - 1);

    // ---- Demon's Gate [BYTES seg6:0x3085-0x30ed]
    const std::string gate = load(s, kStrDemonsGate, kEnterStringMax);
    if (ends_with(cut, gate)) {
        if (!in.game.player) return refuse();  // the demo [BYTES seg6:0x30a1]
        if (state_digit(blank_colon(dir), in.game, records, s) == kStateLocked) {
            h.demons_gate = 0;  // [BYTES seg6:0x30d1 seg6:0x30e1]
            return refuse();
        }
        h.demons_gate = 1;  // [BYTES seg6:0x30e7]
        h.directory = gate;  // [BYTES seg6:0x30ed]
        dir = gate;
    } else {
        // ---- the race and duel gate [BYTES seg6:0x3147-0x31da]
        h.demons_gate = 0;
        if (h.race == 0) {
            const std::string race = load(s, kStrRaceSuffix, kEnterStringMax);
            if (ends_with(cut, race) &&
                (st.demo_toggle == 0 || (in.command_line && in.demo_type == kDemoTypeRace))) {
                h.race = 2;
                if (!in.game.player) st.demo_toggle = 1;
            }
        }
        if (h.duel == 0) {
            const std::string duel = load(s, kStrDuelSuffix, kEnterStringMax);
            if (ends_with(cut, duel) &&
                (st.demo_toggle == 0 || (in.command_line && in.demo_type == kDemoTypeDuel))) {
                h.duel = 2;
                if (!in.game.player) st.demo_toggle = 1;
            }
        }
        if (h.race == 0 && h.duel == 0) {
            st.demo_toggle = 0;  // [BYTES seg6:0x31da]
        } else if (state_digit(blank_colon(dir), in.game, records, s) == kStateLocked) {
            h.race = 0;  // [BYTES seg6:0x321c]
            h.duel = 0;
            return refuse();
        }

        if (h.race == 0 && h.duel == 0) {
            // ---- an ordinary directory: a *.exe and a state below 2 [BYTES seg6:0x3661-0x3683]
            if (!w.has_exe(h.directory, "seg6:0x301a via seg6:0x4190")) return refuse();
            if (state_digit(dir, in.game, records, s) > kStateVisited) return refuse();
        } else if (h.race == 0) {
            // ---- a duel [BYTES seg6:0x34b5-0x3642]
            if (!in.command_line) {
                int i = in.boxes.duelers;
                write("Duelers", i);  // [BYTES seg6:0x351d]
                h.duel = i + 2;
                i = in.boxes.duel_diff;
                write("DuelDiff", i);  // [BYTES seg6:0x3569]
                h.difficulty = i;
                i = in.boxes.duel_opp;
                write("DuelOpp", i);  // [BYTES seg6:0x35b3]
                h.opponent = i;
            } else {
                // Whenever DGROUP:0x90f8 != 0, without a DGROUP:0x8e5e test [BYTES seg6:0x34ca].
                h.difficulty = ini_int(ini, "DuelDiff", 0);
                h.opponent = 2;
                h.duel = 2;
            }
            const std::string tail = cut_suffix(cut, load(s, kStrDuelSuffix, kEnterStringMax));
            for (int i = 0; i < 1; ++i) {  // [BYTES seg6:0x3618 seg6:0x3642]
                const std::string name = load(s, kStrDuel0 + i, kEnterStringMax);
                if (ends_with(tail, name)) {
                    h.duel += i * 4;
                    h.directory = name;
                    dir = name;
                }
            }
        } else {
            // ---- a race [BYTES seg6:0x31d3-0x348d]
            if (!in.command_line || !in.demo) {
                int i = in.boxes.racers;
                write("Racers", i);  // [BYTES seg6:0x3339]
                h.race = i == 0 ? 1 : i + 1;
                i = in.boxes.race_diff;
                write("RaceDiff", i);  // [BYTES seg6:0x3392]
                h.difficulty = i;
                i = in.boxes.race_opp;
                write("RaceOpp", i);  // [BYTES seg6:0x33dc]
                h.opponent = i;
                i = in.boxes.race_laps;
                write("RaceLaps", i);  // [BYTES seg6:0x3426]
                // 1, 5 or 10 laps for index 0, 1, 2; another index leaves DGROUP:0x9ea0
                // [BYTES seg6:0x3434 seg6:0x343e].
                if (i == 0) h.laps = 1;
                else if (i == 1) h.laps = 5;
                else if (i == 2) h.laps = 10;
            } else {
                const int r = ini_int(ini, "DRacers", 0);  // [BYTES seg6:0x3208]
                h.race = r == 0 ? 1 : r + 1;
                h.difficulty = ini_int(ini, "DDiff", 0);  // [BYTES seg6:0x3262]
                h.opponent = 2;
                const int l = ini_int(ini, "DLaps", 0);  // [BYTES seg6:0x327e]
                if (l == 0) {
                    // rand() % 7 by signed idiv: 4 gives 1 lap, 5 gives 10, anything else 5
                    // [BYTES seg6:0x328b-0x32a6].
                    const int m = (rand ? rand() : 0) % 7;
                    h.laps = m == 4 ? 1 : m == 5 ? 10 : 5;
                } else if (l == 1) {
                    h.laps = 1;
                } else if (l == 2) {
                    h.laps = 5;
                } else if (l == 3) {
                    h.laps = 10;
                }
            }
            const std::string tail = cut_suffix(cut, load(s, kStrRaceSuffix, kEnterStringMax));
            for (int i = 0; i < 3; ++i) {  // [BYTES seg6:0x3465 seg6:0x348d]
                const std::string name = load(s, kStrRace0 + i, kEnterStringMax);
                if (ends_with(tail, name)) {
                    h.race += i * 4;
                    h.directory = name;
                    dir = name;
                }
            }
        }
    }

    // ---- on to the acceptance seg6:0x3686 [BYTES seg6:0x3686 seg6:0x368c].
    res.accepted = true;
    res.action = enter_action(true, in.wave, in.restored);
    res.handover = h;
    res.dir = dir;
    return res;
}

EnterResult enter(EnterState& st, std::string& dir, const EnterInputs& in, Walker& w, Strings& s,
                  Records& records, settings::IniFile* ini, const std::function<int()>& rand) {
    EnterResult res = check(st, dir, in, w, s, records, ini, rand);
    // seg6:0x3686: the state request, then the directory record of DGROUP:0x6990 [BYTES
    // seg6:0x36b6].
    if (res.accepted) res.record = records.enter(dir, &res.added);
    return res;
}

WaveKind decode(const Handover& h) {
    WaveKind k;
    if (h.race != 0) {  // [BYTES seg9:0x3db6 seg9:0x3dbd seg9:0x3dd7 seg9:0x3de6]
        k.kind = 6;
        k.opponents = (h.race - 1) % 4;
        k.course = (h.race - 1) / 4;
    }
    if (h.duel != 0) {  // [BYTES seg9:0x3e00 seg9:0x3e10]
        k.kind = 7;
        k.opponents = (h.duel - 1) % 4;
    }
    return k;
}

}  // namespace is::nav
