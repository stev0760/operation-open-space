// MASTER.BOD and MASTER.SHP: field offsets, signedness and the byte-exact writer on
// synthetic records, then the shipped files (IS_ORIGINAL_DIR) against docs/re/formats.md.
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "formats/ne.h"
#include "formats/ships.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {

void put(std::vector<std::uint8_t>& b, std::size_t at, const std::string& s) {
    std::copy(s.begin(), s.end(), b.begin() + static_cast<std::ptrdiff_t>(at));
}

void put16(std::vector<std::uint8_t>& b, std::size_t at, std::uint16_t v) {
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
}

// A body record with a distinct value in every field (TRANSFORMER's, with signed bytes).
std::vector<std::uint8_t> body_bytes() {
    std::vector<std::uint8_t> b(kBodyRecordSize, 0);
    put(b, 0x00, "TRANSFORMER");
    put16(b, 0x15, 1);
    put16(b, 0x17, 0x0302);
    b[0x19] = 0xF8;  // -8
    b[0x1a] = 10;
    b[0x1b] = 0x81;  // -127
    b[0x1c] = 24;
    b[0x1d] = 0xFF;  // -1
    b[0x1e] = 0xC8;  // 200, unsigned
    b[0x1f] = 6;
    b[0x20] = kBodyShield | kBodyFlag40;
    put16(b, 0x21, 60);
    put(b, 0x23, "IDRAGON");
    return b;
}

// A ship record whose number fields hold distinct non-zero bytes, so a field read at
// another field's offset gets a different value: TRANSFORMER's, with team 5, to_friends 1,
// byte_07 12 and both words given a high byte. re/tests/test_ships.py builds the same.
std::vector<std::uint8_t> ship_bytes() {
    std::vector<std::uint8_t> s(kShipRecordSize, 0);
    const std::uint8_t head[10] = {0x21, 0x29, 0x22, 5, 1, 2, 8, 12, 0x0a, 0x15};
    std::copy(head, head + 10, s.begin());
    put(s, 0x0a, "TRANSFORMER");
    put16(s, 0x1f, 0x0307);
    put16(s, 0x21, 0x0423);
    put(s, 0x23, "SDi");
    put(s, 0x38, "ODD");
    return s;
}

// The sum of one number field over every record.
template <class Rec, class Field>
long field_sum(const std::vector<Rec>& recs, Field Rec::*field) {
    long sum = 0;
    for (const auto& r : recs) sum += static_cast<long>(r.*field);
    return sum;
}

template <std::size_t N>
std::vector<std::uint8_t> vec(const std::array<std::uint8_t, N>& a) {
    return {a.begin(), a.end()};
}

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    return fs::path(v);
}

}  // namespace

TEST_CASE("MASTER.BOD record: every field at its offset, with its signedness") {
    const auto raw = body_bytes();
    const BodyRecord b = parse_body_record(raw.data(), raw.size());
    CHECK(b.name == "TRANSFORMER");
    CHECK(b.word_15 == 1);
    CHECK(b.word_17 == 0x0302);
    CHECK(b.byte_19 == -8);
    CHECK(b.byte_1a == 10);
    CHECK(b.strength_base == -127);
    CHECK(b.byte_1c == 24);
    CHECK(b.byte_1d == -1);
    CHECK(b.byte_1e == 200);
    CHECK(b.byte_1f == 6);
    CHECK(b.flags == 0x42);
    CHECK(b.bitmap_id == 60);
    CHECK(b.bitmap_file == "IDRAGON");
    CHECK(vec(write_body_record(b)) == raw);
    CHECK_THROWS_AS(parse_body_record(raw.data(), kBodyRecordSize - 1), FormatError);
}

TEST_CASE("MASTER.SHP record: every field at its offset") {
    const auto raw = ship_bytes();
    const ShipRecord s = parse_ship_record(raw.data(), raw.size());
    CHECK(s.fuel_tank == 33);
    CHECK(s.thruster == 41);
    CHECK(s.armor == 34);
    CHECK(s.team == 5);
    CHECK(s.to_friends == 1);
    CHECK(s.to_enemies == 2);
    CHECK(s.weapon == 8);
    CHECK(s.byte_07 == 12);
    CHECK(s.byte_08 == 10);
    CHECK(s.flags == (kShipCombo180 | kShipRepair | kShipCapturesIcons));
    CHECK(s.name == "TRANSFORMER");
    CHECK(s.hit_sound == 0x0307);
    CHECK(s.body == 0x0423);
    CHECK(s.author == "SDi");
    CHECK(s.sound_file == "ODD");
    CHECK(vec(write_ship_record(s)) == raw);
    CHECK_THROWS_AS(parse_ship_record(raw.data(), kShipRecordSize - 1), FormatError);
}

TEST_CASE("ship strings: stale bytes after the NUL, full-width text, limits") {
    auto raw = ship_bytes();
    raw[0x0a + 15] = 'Q';  // after "TRANSFORMER\0"
    raw[0x40] = 0xEE;      // the last byte of the sound file field
    ShipRecord s = parse_ship_record(raw.data(), raw.size());
    CHECK(s.name == "TRANSFORMER");
    CHECK(s.sound_file == "ODD");
    CHECK(s.stale[0x0a + 15] == 'Q');
    CHECK(s.stale[0x40] == 0xEE);
    CHECK(static_cast<std::size_t>(std::count(s.stale.begin(), s.stale.end(), 0)) ==
          kShipRecordSize - 2);
    CHECK(vec(write_ship_record(s)) == raw);

    // a cleared `stale` gives the Factory's zero-filled record
    s.stale = {};
    const auto clean = write_ship_record(s);
    CHECK(clean[0x0a + 15] == 0);
    CHECK(clean[0x40] == 0);

    // a string that fills its field has no NUL and round-trips
    s.author = std::string(kShipAuthorSize, 'A');
    const auto full = write_ship_record(s);
    CHECK(full[0x37] == 'A');
    CHECK(full[0x38] == 'O');  // the next field is untouched
    CHECK(parse_ship_record(full.data(), full.size()).author == s.author);

    // one byte more, or an embedded NUL, cannot be written
    s.author.push_back('A');
    CHECK_THROWS_AS(write_ship_record(s), FormatError);
    s.author = std::string("a\0b", 3);
    CHECK_THROWS_AS(write_ship_record(s), FormatError);

    BodyRecord b;
    b.bitmap_file = "TOOLONGNAME";
    CHECK_THROWS_AS(write_body_record(b), FormatError);
    b.bitmap_file = "IDRAGON";
    b.name = "cp1252 \xE9";  // bytes pass through unconverted
    const auto rb = write_body_record(b);
    CHECK(rb[0x07] == 0xE9);
    CHECK(rb[0x08] == 0);
    CHECK(parse_body_record(rb.data(), rb.size()).name == b.name);
}

TEST_CASE("MASTER.BOD/SHP files: count = length / record size, the tail is kept") {
    std::vector<std::uint8_t> bod = body_bytes();
    auto second = body_bytes();
    second[0] = 0;  // a free record
    bod.insert(bod.end(), second.begin(), second.end());
    const std::vector<std::uint8_t> tail = {0, 0, 0xAB, 0, 0};
    bod.insert(bod.end(), tail.begin(), tail.end());
    const MasterBod fb = parse_master_bod(bod.data(), bod.size());
    REQUIRE(fb.records.size() == 2);
    CHECK(fb.records[1].name.empty());
    CHECK(fb.records[1].stale[1] == 'R');  // "\0RANSFORMER": all stale
    CHECK(fb.tail == tail);
    CHECK(write_master_bod(fb) == bod);

    std::vector<std::uint8_t> shp = ship_bytes();
    shp.resize(kShipRecordSize + kShipRecordSize - 1, 0x5A);  // a partial record is tail
    const MasterShp fshp = parse_master_shp(shp.data(), shp.size());
    CHECK(fshp.records.size() == 1);
    CHECK(fshp.tail.size() == kShipRecordSize - 1);
    CHECK(write_master_shp(fshp) == shp);

    const MasterShp empty = parse_master_shp(nullptr, 0);
    CHECK(empty.records.empty());
    CHECK(empty.tail.empty());
    CHECK(write_master_shp(empty).empty());

    MasterBod made;  // a file built from scratch: zero-filled records, no tail
    made.records.resize(3);
    made.records[0].name = "SKULL";
    const auto out = write_master_bod(made);
    CHECK(out.size() == 3 * kBodyRecordSize);
    CHECK(static_cast<std::size_t>(std::count(out.begin(), out.end(), 0)) ==
          3 * kBodyRecordSize - 5);
}

TEST_CASE("MASTER.BOD and MASTER.SHP golden: the shipped files") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const auto bod_bytes = read_file(dir / "MASTER.BOD");
    const auto shp_bytes = read_file(dir / "MASTER.SHP");
    const MasterBod bod = parse_master_bod(bod_bytes.data(), bod_bytes.size());
    const MasterShp shp = parse_master_shp(shp_bytes.data(), shp_bytes.size());

    // formats.md: 52 records and 16 zero bytes; 118 records and 10 zero bytes
    CHECK(bod_bytes.size() == 2304);
    REQUIRE(bod.records.size() == 52);
    CHECK(bod.tail == std::vector<std::uint8_t>(16, 0));
    CHECK(shp_bytes.size() == 7680);
    REQUIRE(shp.records.size() == 118);
    CHECK(shp.tail == std::vector<std::uint8_t>(10, 0));
    CHECK(write_master_bod(bod) == bod_bytes);
    CHECK(write_master_shp(shp) == shp_bytes);

    // every shipped body holds 1 at +0x15; only the last record is free, and it names
    // the bitmap file IDRAGON (theworks/IDRAGON.DIB) with id 0xffff
    std::size_t free_records = 0;
    for (const auto& b : bod.records) {
        CHECK(b.word_15 == 1);
        free_records += b.name.empty();
        CHECK(std::all_of(b.stale.begin(), b.stale.end(), [](std::uint8_t c) { return c == 0; }));
    }
    CHECK(free_records == 1);
    const BodyRecord& last = bod.records.back();
    CHECK(last.name.empty());
    CHECK(last.bitmap_id == kBodyBitmapFromFile);
    CHECK(last.bitmap_file == "IDRAGON");

    // the shipped thrusters span 23-60; every body index names a record
    unsigned lo = 255, hi = 0;
    for (const auto& s : shp.records) {
        lo = std::min<unsigned>(lo, s.thruster);
        hi = std::max<unsigned>(hi, s.thruster);
        CHECK(s.body < bod.records.size());
        CHECK(std::all_of(s.stale.begin(), s.stale.end(), [](std::uint8_t c) { return c == 0; }));
    }
    CHECK(lo == 23);
    CHECK(hi == 60);

    // the Transformer: SHP record 1 uses body 33, whose bitmap is IGRAPHIC.DLL #4000 60
    const ShipRecord& t = shp.records[1];
    CHECK(t.name == "TRANSFORMER");
    CHECK(t.author == "SDi");
    CHECK(t.sound_file == "ODD");
    CHECK(t.hit_sound == 7);
    CHECK(t.fuel_tank == 33);
    CHECK(t.thruster == 41);
    CHECK(t.armor == 34);
    CHECK(t.body == 33);
    CHECK(bod.records[33].name == "TRANSFORMER");
    CHECK(bod.records[33].bitmap_id == 60);
    CHECK(bod.records[33].bitmap_file.empty());

    // THUNDERBIRD, SHP record 3, holds distinct non-zero bytes in every number field:
    // 1b 32 20 05 02 01 03 07 0c 1c
    const ShipRecord& tb = shp.records[3];
    CHECK(tb.fuel_tank == 27);
    CHECK(tb.thruster == 50);
    CHECK(tb.armor == 32);
    CHECK(tb.team == 5);
    CHECK(tb.to_friends == 2);
    CHECK(tb.to_enemies == 1);
    CHECK(tb.weapon == 3);
    CHECK(tb.byte_07 == 7);
    CHECK(tb.byte_08 == 12);
    CHECK(tb.flags == (kShipRepair | kShipCloak | kShipCapturesIcons));
    CHECK(tb.name == "THUNDERBIRD");
    CHECK(tb.hit_sound == 9);
    CHECK(tb.body == 3);
    CHECK(tb.author == "SDi");
    CHECK(tb.sound_file == "FUNK2");
    CHECK(bod.records[3].name == "THUNDERBIRD");

    // Each number field summed over every record, as `isre ships` reads the same files
    // (re/tests/test_ships.py holds the same sums): a field read at a wrong offset changes
    // its sum.
    CHECK(field_sum(bod.records, &BodyRecord::word_15) == 52);
    CHECK(field_sum(bod.records, &BodyRecord::word_17) == 52);
    CHECK(field_sum(bod.records, &BodyRecord::byte_19) == 1283);
    CHECK(field_sum(bod.records, &BodyRecord::byte_1a) == 495);
    CHECK(field_sum(bod.records, &BodyRecord::strength_base) == 1557);
    CHECK(field_sum(bod.records, &BodyRecord::byte_1c) == 1563);
    CHECK(field_sum(bod.records, &BodyRecord::byte_1d) == 302);
    CHECK(field_sum(bod.records, &BodyRecord::byte_1e) == 212);
    CHECK(field_sum(bod.records, &BodyRecord::byte_1f) == 707);
    CHECK(field_sum(bod.records, &BodyRecord::flags) == 492);
    CHECK(field_sum(bod.records, &BodyRecord::bitmap_id) == 97170);
    CHECK(field_sum(shp.records, &ShipRecord::fuel_tank) == 3462);
    CHECK(field_sum(shp.records, &ShipRecord::thruster) == 5135);
    CHECK(field_sum(shp.records, &ShipRecord::armor) == 3705);
    CHECK(field_sum(shp.records, &ShipRecord::team) == 390);
    CHECK(field_sum(shp.records, &ShipRecord::to_friends) == 186);
    CHECK(field_sum(shp.records, &ShipRecord::to_enemies) == 166);
    CHECK(field_sum(shp.records, &ShipRecord::weapon) == 1096);
    CHECK(field_sum(shp.records, &ShipRecord::byte_07) == 935);
    CHECK(field_sum(shp.records, &ShipRecord::byte_08) == 925);
    CHECK(field_sum(shp.records, &ShipRecord::flags) == 2633);
    CHECK(field_sum(shp.records, &ShipRecord::hit_sound) == 1161);
    CHECK(field_sum(shp.records, &ShipRecord::body) == 2767);

    // every body bitmap id other than 0xffff is an IGRAPHIC.DLL #4000 resource
    const NeModule ig = NeModule::load(dir / "IGRAPHIC.DLL");
    std::set<std::uint16_t> ids;
    for (const auto& b : bod.records)
        if (b.bitmap_id != kBodyBitmapFromFile) {
            ids.insert(b.bitmap_id);
            CHECK_MESSAGE(ig.find(4000, b.bitmap_id) != nullptr, b.name << " " << b.bitmap_id);
        }
    CHECK(ids.size() == 51);
}
