// Navigation's fields of a 0x297-byte directory record in the record's bytes (formats.md
// "Directory records", nav.md "Directory records"). The game keeps one byte store of the
// records, world::World::records (DGROUP:0x69f4, the block a saved game writes); nav::Records is
// Navigation's view of it, copied in at the window's open and written back at Enter and before a
// save (src/dialogs/save_game.h sync_*) [DESIGN ADR 0014 decision 1]. Portable.
#pragma once

#include <cstdint>

#include "nav/tree.h"

namespace is::nav {

constexpr int kDirPath = 0x152;       // 110 bytes [BYTES seg6:0x36e4 seg6:0x377f]
constexpr int kDirPathBytes = 110;
constexpr int kDirDestroyed = 0x14e;  // [BYTES seg6:0x5b20]
constexpr int kDirCaptured = 0x150;   // [BYTES seg6:0x5b20]
constexpr int kDirState = 0x1d0;      // [BYTES seg6:0x598c]

// The four fields from a record's bytes `b` (0x297 of them); the path stops at its NUL or at
// 109 bytes.
DirRecord dir_from_bytes(const std::uint8_t* b);
// The path (at most 109 bytes and a NUL, the rest of the 110 zero) [BYTES seg6:0x3788].
void dir_path_to_bytes(const std::string& path, std::uint8_t* b);
// All four fields into the bytes.
void dir_to_bytes(const DirRecord& r, std::uint8_t* b);

}  // namespace is::nav
