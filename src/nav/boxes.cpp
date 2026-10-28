// The RACE and DUEL boxes: the portable part (see boxes.h).
#include "nav/boxes.h"

#include "settings/settings.h"

namespace is::nav {

const std::array<Combo, 7>& combos() {
    // seg6:0x23d4's creation order [BYTES seg6:0x26ab seg6:0x27b6 seg6:0x2895 seg6:0x2974
    // seg6:0x2a53 seg6:0x2b32 seg6:0x2c7e]; the LoadString ids 0x863-0x879 [BYTES seg6:0x26da
    // seg6:0x27e5 seg6:0x28c4 seg6:0x29a3].
    static const std::array<Combo, 7> t = {{
        {kRacersId, "Racers", 1, {2147, 2148, 2149, 2150}, BoxKind::kRace},
        {kDuelersId, "Duelers", 0, {2151, 2152, 2153}, BoxKind::kDuel},
        {kRaceDiffId, "RaceDiff", 0, {2154, 2155, 2156}, BoxKind::kRace},
        {kDuelDiffId, "DuelDiff", 0, {2154, 2155, 2156}, BoxKind::kDuel},
        {kRaceLapsId, "RaceLaps", 1, {2157, 2158, 2159}, BoxKind::kRace},
        {kRaceOppId, "RaceOpp", 2, {2160, 2161, 2162, 2163, 2164, 2165}, BoxKind::kRace},
        {kDuelOppId, "DuelOpp", 2, {2166, 2167, 2162, 2163, 2168, 2169}, BoxKind::kDuel},
    }};
    return t;
}

const Combo* combo(int id) {
    for (const auto& c : combos())
        if (c.id == id) return &c;
    return nullptr;
}

std::vector<int> box_controls(BoxKind kind) {
    switch (kind) {
    case BoxKind::kRace: return {kRaceBoxId, kRacersId, kRaceDiffId, kRaceLapsId, kRaceOppId};
    case BoxKind::kDuel: return {kDuelBoxId, kDuelersId, kDuelDiffId, kDuelOppId};
    case BoxKind::kNone: break;
    }
    return {};
}

int show_command(BoxKind kind, int id) {
    // local_6 (RACE) and local_8 (DUEL) are 5 for the shown box and 0 otherwise [BYTES
    // seg6:0x2dba].
    const bool race = id == kRaceBoxId || id == kRacersId || id == kRaceDiffId ||
                      id == kRaceLapsId || id == kRaceOppId;
    const bool duel = id == kDuelBoxId || id == kDuelersId || id == kDuelDiffId || id == kDuelOppId;
    if (race) return kind == BoxKind::kRace ? 5 : 0;
    if (duel) return kind == BoxKind::kDuel ? 5 : 0;
    return 0;
}

int initial_index(const settings::IniFile* ini, const Combo& c) {
    const int v = ini ? ini->get_int(settings::kGameSection, c.key, c.def) : c.def;
    // CB_SETCURSEL with an index outside the list selects nothing [REF Win16 SDK CB_SETCURSEL].
    // What Win16 GetPrivateProfileInt returns for a negative value is [OPEN]; the port reads it
    // as negative, which selects nothing.
    const int n = static_cast<int>(c.strings.size());
    return v >= 0 && v < n ? v : -1;
}

Box create_box(int id, int cw, int ext) {
    switch (id) {
    case kRaceBoxId:
    case kDuelBoxId: return {cw - 0x5a, 0x78, 0x5a, 0x28};  // [BYTES seg6:0x25b6 seg6:0x2614]
    case kRacersId:
    case kDuelersId: return {cw - 0x3c, 0x78, 0x3c, (ext + 10) * 3};  // [BYTES seg6:0x26ab seg6:0x27b6]
    case kRaceDiffId:
    case kDuelDiffId: return {cw - 0x3c, 0x96, 0x3c, (ext + 10) * 3};  // [BYTES seg6:0x2895 seg6:0x2974]
    case kRaceLapsId: return {cw - 0x3c, 0xb4, 0x3c, (ext + 10) * 3};  // [BYTES seg6:0x2a53]
    case kRaceOppId:
    case kDuelOppId: return {cw - 0x3c, 0xb4, 0x3c, (ext + 5) * 6};  // [BYTES seg6:0x2b32 seg6:0x2c7e]
    default: return {};
    }
}

Box size_box(int id, int cw, int ch, int ext) {
    const int y0 = ch - 0x9b;  // [BYTES seg6:0x211e]
    switch (id) {
    case kRaceBoxId:
    case kDuelBoxId: return {cw - 0x6e, y0, 0x66, 0x7a};  // [BYTES seg6:0x2141 seg6:0x2163]
    case kRacersId: return {cw - 0x61, y0 + 0x12, 0x50, ext * 4 + 0x1e};  // [BYTES seg6:0x2194]
    case kDuelersId: return {cw - 0x63, y0 + 0x12, 0x53, (ext + 10) * 3};  // [BYTES seg6:0x21bc]
    case kRaceDiffId: return {cw - 0x61, y0 + 0x2b, 0x50, (ext + 10) * 3};  // [BYTES seg6:0x21ed]
    case kDuelDiffId: return {cw - 0x63, y0 + 0x2b, 0x53, (ext + 10) * 3};  // [BYTES seg6:0x2215]
    case kRaceLapsId: return {cw - 0x61, y0 + 0x44, 0x50, (ext + 10) * 3};  // [BYTES seg6:0x2246]
    case kRaceOppId: return {cw - 0x61, y0 + 0x5d, 0x50, (ext + 5) * 6};  // [BYTES seg6:0x2274]
    case kDuelOppId: return {cw - 0x63, y0 + 0x44, 0x53, (ext + 5) * 6};  // [BYTES seg6:0x229c]
    default: return {};
    }
}

int& BoxValues::at(int id) {
    switch (id) {
    case kRacersId: return racers;
    case kRaceDiffId: return race_diff;
    case kRaceLapsId: return race_laps;
    case kRaceOppId: return race_opp;
    case kDuelersId: return duelers;
    case kDuelDiffId: return duel_diff;
    default: return duel_opp;
    }
}

int BoxValues::at(int id) const { return const_cast<BoxValues*>(this)->at(id); }

BoxValues initial_values(const settings::IniFile* ini) {
    BoxValues v;
    for (const auto& c : combos()) v.at(c.id) = initial_index(ini, c);
    return v;
}

}  // namespace is::nav
