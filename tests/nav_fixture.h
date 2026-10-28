// The Navigation tests' world: an in-memory replica of oracle fixture `basic`
// (oracle/fixtures/make_fixtures.py) as drive D:, its icon counts, and the ENG_LANG.DLL strings
// the tree and the status line load (copied from `isre res` strings.tsv; the port loads them by
// id from the user's ENG_LANG.DLL).
#pragma once

#include <map>
#include <string>

#include "nav/files.h"
#include "nav/fs.h"
#include "nav/tree.h"

namespace navtest {

class FakeStrings : public is::nav::Strings {
public:
    std::map<int, std::string> s = {
        {736, " - DUEL"},
        {739, " - RACE"},
        {740, "Lazy Loop"},
        {741, "SDi Speedway"},
        {742, "Grand Prix"},
        {750, "Dodge City"},
        {760, "DEMON'S GATE"},
        {761, "Gateway to the ultimate challenge!"},
        {762, "Enter when you have what it takes.."},
        {763, "This is an Inner Space race track"},
        {764, "Enter here for a thrill!"},
        {765, "This directory is complete"},
        {766, "This is the arena for Inner Space duels"},
        {767, "If you have a dispute, take him here!"},
        {797, " AND"},
        {938, "No apps"},
        {939, "Select another directory to enter."},
        {940, "THERE"},
        {941, "FILE"},
        {942, " IN THIS DIRECTORY"},
        {943, "YOU HAVE NOT BEEN HERE BEFORE"},
        {2055, "THERE IS"},
        {2056, "THERE ARE"},
        {2323, "To enter, you must pay for the full game version."},
        {2506, "ICON WAS"},
        {2515, "ICON"},
        {3028, "%? %# IN %#."},
        {3029, "%# CAPTURED AND %# DESTROYED."},
        {10941, "FILES"},
        {12506, "ICONS WERE"},
        {12515, "ICONS"},
    };
    std::string lang(int id) override {
        const auto it = s.find(id);
        return it == s.end() ? std::string() : it->second;
    }
};

// GAMES/ALPHA.EXE [icon 0], BRAVO.EXE [1], CHARLIE.EXE [2]; TOOLS/DELTA.EXE [3], ECHO.EXE
// [4, 5, 6]; EMPTY; DEEP/A/B/FOXTROT.EXE [7], in the order the oracle's drive enumerates them
// (newgame-0002 trace: DEEP, EMPTY, GAMES, TOOLS).
// `d` is the tree's base: the drive root "D:\" or a directory with its '\'.
inline void build_basic(is::nav::MemFs& fs, const std::string& d = "D:\\") {
    fs.add_drive(d[0], is::nav::DriveType::kFixed);
    fs.add_dir(d + "DEEP");
    fs.add_dir(d + "DEEP\\A");
    fs.add_dir(d + "DEEP\\A\\B");
    fs.add_file(d + "DEEP\\A\\B\\FOXTROT.EXE");
    fs.add_dir(d + "EMPTY");
    fs.add_dir(d + "GAMES");
    fs.add_file(d + "GAMES\\ALPHA.EXE");
    fs.add_file(d + "GAMES\\BRAVO.EXE");
    fs.add_file(d + "GAMES\\CHARLIE.EXE");
    fs.add_dir(d + "TOOLS");
    fs.add_file(d + "TOOLS\\DELTA.EXE");
    fs.add_file(d + "TOOLS\\ECHO.EXE");
}

// The group-icon counts of the fixture programs (what the port's reader and the oracle's
// patched ExtractIcon(-1) return, wave-0001).
inline void basic_icons(is::nav::MemIcons& icons, const std::string& d = "D:\\") {
    icons.set(d + "GAMES\\ALPHA.EXE", 1);
    icons.set(d + "GAMES\\BRAVO.EXE", 1);
    icons.set(d + "GAMES\\CHARLIE.EXE", 1);
    icons.set(d + "TOOLS\\DELTA.EXE", 1);
    icons.set(d + "TOOLS\\ECHO.EXE", 3);
    icons.set(d + "DEEP\\A\\B\\FOXTROT.EXE", 1);
}

// The call list as log lines, for readable comparisons.
inline std::vector<std::string> lines(const std::vector<is::nav::FsCall>& calls) {
    std::vector<std::string> v;
    for (const auto& c : calls) v.push_back(is::nav::describe(c));
    return v;
}

}  // namespace navtest
