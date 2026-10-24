// MASTER.BOD (ship bodies) and MASTER.SHP (ships): arrays of fixed-size records with no
// header, followed by the padding of the writer's rounded global block. The layouts and
// what each field means: docs/re/formats.md, sections "MASTER.BOD: ship bodies" and
// "MASTER.SHP: ships". This reader only maps bytes to fields and back; it computes nothing
// from the values (the design record that INSPACE seg14:0x1b84 builds is game code).
//
// The writer reproduces the input byte for byte: the records, the bytes after each string's
// NUL and the tail. The Ship Factory writes both files (M8). Its writer copies the fields
// into a zero-filled block (formats.md), which is what write_* gives for records whose
// `stale` is all zero, as in a default-constructed record.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "formats/bytes.h"

namespace is::fmt {

// ---- MASTER.BOD: 44-byte (0x2c) records

constexpr std::size_t kBodyRecordSize = 0x2c;
constexpr std::size_t kBodyNameSize = 21;        // +0x00
constexpr std::size_t kBodyBitmapFileSize = 9;   // +0x23

// BodyRecord::bitmap_id when the bitmap is a file named by bitmap_file.
constexpr std::uint16_t kBodyBitmapFromFile = 0xFFFF;

// BodyRecord::flags. The Import Ship Body dialog (ENG_LANG.DLL DIALOG 300) sets one bit per
// checkbox; Body Thrust excludes the other three.
enum : std::uint8_t {
    kBodyThrust = 0x01,          // checkbox 301 Body Thrust
    kBodyShield = 0x02,          // 310 Shield
    kBodyPullOffWings = 0x04,    // 302 Pull-off Wings
    kBodyThrustBehind = 0x08,    // 309 Thrust Behind Ship
    kBodyFlag40 = 0x40,          // no checkbox; what it means is [OPEN]
};

struct BodyRecord {
    std::string name;              // +0x00 cp1252, 21 bytes; empty marks a free record
    std::uint16_t word_15 = 0;     // +0x15 [OPEN]; every shipped record holds 1
    std::uint16_t word_17 = 0;     // +0x17 [OPEN]; read like +0x15
    std::int8_t byte_19 = 0;       // +0x19 [OPEN]; import: bitmap measure - 8
    std::int8_t byte_1a = 0;       // +0x1a [OPEN]; import: 10
    std::int8_t strength_base = 0; // +0x1b strength base
    std::int8_t byte_1c = 0;       // +0x1c [OPEN]; reaches ship-record word +0x0e
    std::int8_t byte_1d = 0;       // +0x1d [OPEN] name; feeds ship-record word +0x14
    std::uint8_t byte_1e = 0;      // +0x1e [OPEN]; import: 0, no INSPACE reader found
    std::uint8_t byte_1f = 0;      // +0x1f [OPEN]; import: bitmap measure - 10
    std::uint8_t flags = 0;        // +0x20 kBody*
    std::uint16_t bitmap_id = 0;   // +0x21 IGRAPHIC.DLL #4000 id, or kBodyBitmapFromFile
    std::string bitmap_file;       // +0x23 cp1252, 9 bytes: bitmap base name, ".DIB" stripped
    // The bytes after each string's NUL, at their record offsets; every other byte is 0.
    std::array<std::uint8_t, kBodyRecordSize> stale{};
};

struct MasterBod {
    std::vector<BodyRecord> records;  // file length / 44, as every reader counts them
    std::vector<std::uint8_t> tail;   // the bytes after the last whole record (the shipped
                                      // file: 16 zero bytes); readers ignore them
};

// `n` must be at least kBodyRecordSize.
BodyRecord parse_body_record(const std::uint8_t* p, std::size_t n);
// Throws FormatError when a string does not fit its field or holds a NUL.
std::array<std::uint8_t, kBodyRecordSize> write_body_record(const BodyRecord& r);

MasterBod parse_master_bod(const std::uint8_t* p, std::size_t n);
std::vector<std::uint8_t> write_master_bod(const MasterBod& f);

// ---- MASTER.SHP: 65-byte (0x41) records

constexpr std::size_t kShipRecordSize = 0x41;
constexpr std::size_t kShipNameSize = 21;        // +0x0a
constexpr std::size_t kShipAuthorSize = 21;      // +0x23
constexpr std::size_t kShipSoundFileSize = 9;    // +0x38

// ShipRecord::hit_sound when the sound is a WAV file named by sound_file.
constexpr std::uint16_t kShipSoundFromFile = 0xFFFF;

// ShipRecord::flags.
enum : std::uint8_t {
    kShipCombo180 = 0x01,       // Factory combo 180 at a non-zero index; meaning [OPEN]
    kShipRepair = 0x04,         // Repair System, checkbox 144
    kShipCloak = 0x08,          // Cloak, checkbox 145
    kShipCapturesIcons = 0x10,  // Captures Icons, checkbox 143
    kShipGenderMale = 0x20,     // the Factory's hidden &Gender menu: 498 &Male sets it,
                                // 499 &Female clears it
};

struct ShipRecord {
    std::uint8_t fuel_tank = 0;   // +0x00 Fuel Tank (Ship Handling scrollbar 142)
    std::uint8_t thruster = 0;    // +0x01 Thruster (scrollbar 141)
    std::uint8_t armor = 0;       // +0x02 Armor (scrollbar 140)
    std::uint8_t team = 0;        // +0x03 0-based index into [Teams] Team1..Team8
    std::uint8_t to_friends = 0;  // +0x04 0 LOYAL, 1 HELPFUL, 2 FRIENDLY, 3 DISINTERESTED
    std::uint8_t to_enemies = 0;  // +0x05 0 PSYCHOPATHIC, 1 HOSTILE, 2 UNFRIENDLY,
                                  //       3 DISINTERESTED
    std::uint8_t weapon = 0;      // +0x06 initial weapon 0-21: ENG_LANG string 186 + n
    std::uint8_t byte_07 = 0;     // +0x07 [OPEN]; Factory list control 170
    std::uint8_t byte_08 = 0;     // +0x08 [OPEN]; Factory control 230
    std::uint8_t flags = 0;       // +0x09 kShip*
    std::string name;             // +0x0a cp1252, 21 bytes
    std::uint16_t hit_sound = 0;  // +0x1f sound id, or kShipSoundFromFile
    std::uint16_t body = 0;       // +0x21 MASTER.BOD record index
    std::string author;           // +0x23 cp1252, 21 bytes
    std::string sound_file;       // +0x38 cp1252, 9 bytes: WAV base name, ".WAV" stripped
    // The bytes after each string's NUL, at their record offsets; every other byte is 0.
    std::array<std::uint8_t, kShipRecordSize> stale{};
};

struct MasterShp {
    std::vector<ShipRecord> records;  // file length / 65
    std::vector<std::uint8_t> tail;   // the shipped file: 10 zero bytes
};

ShipRecord parse_ship_record(const std::uint8_t* p, std::size_t n);
std::array<std::uint8_t, kShipRecordSize> write_ship_record(const ShipRecord& r);

MasterShp parse_master_shp(const std::uint8_t* p, std::size_t n);
std::vector<std::uint8_t> write_master_shp(const MasterShp& f);

// [OPEN] The tail of a file written from scratch is the writer's GlobalSize rounding: Wine
// rounds to 32 bytes, the Windows 3.x granularity is open (formats.md, Conventions). The
// writer emits `tail` as given and does not compute one.

}  // namespace is::fmt
