#include "formats/ships.h"

#include <algorithm>

namespace is::fmt {

namespace {

// A fixed-size string field: the text up to the first NUL (all `len` bytes when there is
// none); the bytes after the NUL go to `stale` at the same offsets.
std::string get_text(const std::uint8_t* rec, std::size_t off, std::size_t len,
                     std::uint8_t* stale) {
    const std::uint8_t* f = rec + off;
    const std::size_t k = static_cast<std::size_t>(std::find(f, f + len, 0) - f);
    for (std::size_t i = k + 1; i < len; ++i) stale[off + i] = f[i];
    return std::string(reinterpret_cast<const char*>(f), k);
}

// The text, then a NUL when it is shorter than the field; later bytes keep what the
// buffer holds (the stale bytes).
void put_text(std::uint8_t* rec, std::size_t off, std::size_t len, const std::string& s,
              const char* what) {
    if (s.size() > len)
        throw FormatError(std::string(what) + " longer than " + std::to_string(len) + " bytes");
    if (s.find('\0') != std::string::npos) throw FormatError(std::string(what) + " holds a NUL");
    std::copy(s.begin(), s.end(), rec + off);
    if (s.size() < len) rec[off + s.size()] = 0;
}

void put16(std::uint8_t* rec, std::size_t off, std::uint16_t v) {
    rec[off] = static_cast<std::uint8_t>(v);
    rec[off + 1] = static_cast<std::uint8_t>(v >> 8);
}

void need(std::size_t n, std::size_t size) {
    if (n < size) throw FormatError("record past end");
}

template <class Rec, std::size_t Size, class Parse>
std::vector<Rec> parse_records(const std::uint8_t* p, std::size_t n, Parse parse,
                               std::vector<std::uint8_t>& tail) {
    const std::size_t count = n / Size;
    std::vector<Rec> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) out.push_back(parse(p + i * Size, Size));
    tail.assign(p + count * Size, p + n);
    return out;
}

}  // namespace

BodyRecord parse_body_record(const std::uint8_t* p, std::size_t n) {
    need(n, kBodyRecordSize);
    BodyRecord b;
    Reader r(p, kBodyRecordSize, 0x15);
    b.name = get_text(p, 0x00, kBodyNameSize, b.stale.data());
    b.word_15 = r.u16();
    b.word_17 = r.u16();
    b.byte_19 = static_cast<std::int8_t>(r.u8());
    b.byte_1a = static_cast<std::int8_t>(r.u8());
    b.strength_base = static_cast<std::int8_t>(r.u8());
    b.byte_1c = static_cast<std::int8_t>(r.u8());
    b.byte_1d = static_cast<std::int8_t>(r.u8());
    b.byte_1e = r.u8();
    b.byte_1f = r.u8();
    b.flags = r.u8();
    b.bitmap_id = r.u16();
    b.bitmap_file = get_text(p, 0x23, kBodyBitmapFileSize, b.stale.data());
    return b;
}

std::array<std::uint8_t, kBodyRecordSize> write_body_record(const BodyRecord& b) {
    std::array<std::uint8_t, kBodyRecordSize> o = b.stale;
    put_text(o.data(), 0x00, kBodyNameSize, b.name, "body name");
    put16(o.data(), 0x15, b.word_15);
    put16(o.data(), 0x17, b.word_17);
    o[0x19] = static_cast<std::uint8_t>(b.byte_19);
    o[0x1a] = static_cast<std::uint8_t>(b.byte_1a);
    o[0x1b] = static_cast<std::uint8_t>(b.strength_base);
    o[0x1c] = static_cast<std::uint8_t>(b.byte_1c);
    o[0x1d] = static_cast<std::uint8_t>(b.byte_1d);
    o[0x1e] = b.byte_1e;
    o[0x1f] = b.byte_1f;
    o[0x20] = b.flags;
    put16(o.data(), 0x21, b.bitmap_id);
    put_text(o.data(), 0x23, kBodyBitmapFileSize, b.bitmap_file, "body bitmap file");
    return o;
}

MasterBod parse_master_bod(const std::uint8_t* p, std::size_t n) {
    MasterBod f;
    f.records = parse_records<BodyRecord, kBodyRecordSize>(p, n, parse_body_record, f.tail);
    return f;
}

std::vector<std::uint8_t> write_master_bod(const MasterBod& f) {
    std::vector<std::uint8_t> out;
    out.reserve(f.records.size() * kBodyRecordSize + f.tail.size());
    for (const auto& b : f.records) {
        const auto rec = write_body_record(b);
        out.insert(out.end(), rec.begin(), rec.end());
    }
    out.insert(out.end(), f.tail.begin(), f.tail.end());
    return out;
}

ShipRecord parse_ship_record(const std::uint8_t* p, std::size_t n) {
    need(n, kShipRecordSize);
    ShipRecord s;
    Reader r(p, kShipRecordSize);
    s.fuel_tank = r.u8();
    s.thruster = r.u8();
    s.armor = r.u8();
    s.team = r.u8();
    s.to_friends = r.u8();
    s.to_enemies = r.u8();
    s.weapon = r.u8();
    s.byte_07 = r.u8();
    s.byte_08 = r.u8();
    s.flags = r.u8();
    s.name = get_text(p, 0x0a, kShipNameSize, s.stale.data());
    r.seek(0x1f);
    s.hit_sound = r.u16();
    s.body = r.u16();
    s.author = get_text(p, 0x23, kShipAuthorSize, s.stale.data());
    s.sound_file = get_text(p, 0x38, kShipSoundFileSize, s.stale.data());
    return s;
}

std::array<std::uint8_t, kShipRecordSize> write_ship_record(const ShipRecord& s) {
    std::array<std::uint8_t, kShipRecordSize> o = s.stale;
    o[0x00] = s.fuel_tank;
    o[0x01] = s.thruster;
    o[0x02] = s.armor;
    o[0x03] = s.team;
    o[0x04] = s.to_friends;
    o[0x05] = s.to_enemies;
    o[0x06] = s.weapon;
    o[0x07] = s.byte_07;
    o[0x08] = s.byte_08;
    o[0x09] = s.flags;
    put_text(o.data(), 0x0a, kShipNameSize, s.name, "ship name");
    put16(o.data(), 0x1f, s.hit_sound);
    put16(o.data(), 0x21, s.body);
    put_text(o.data(), 0x23, kShipAuthorSize, s.author, "ship author");
    put_text(o.data(), 0x38, kShipSoundFileSize, s.sound_file, "ship sound file");
    return o;
}

MasterShp parse_master_shp(const std::uint8_t* p, std::size_t n) {
    MasterShp f;
    f.records = parse_records<ShipRecord, kShipRecordSize>(p, n, parse_ship_record, f.tail);
    return f;
}

std::vector<std::uint8_t> write_master_shp(const MasterShp& f) {
    std::vector<std::uint8_t> out;
    out.reserve(f.records.size() * kShipRecordSize + f.tail.size());
    for (const auto& s : f.records) {
        const auto rec = write_ship_record(s);
        out.insert(out.end(), rec.begin(), rec.end());
    }
    out.insert(out.end(), f.tail.begin(), f.tail.end());
    return out;
}

}  // namespace is::fmt
