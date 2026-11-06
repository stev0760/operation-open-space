// The saved-game files (savegame.h). Portable.
#include "formats/savegame.h"

#include <algorithm>
#include <cstring>

namespace is::fmt::save {

namespace {

std::uint16_t rd16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::int16_t rds16(const std::uint8_t* p) { return static_cast<std::int16_t>(rd16(p)); }
std::uint32_t rd32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(rd16(p)) | (static_cast<std::uint32_t>(rd16(p + 2)) << 16);
}
void wr16(std::uint8_t* p, int v) {
    p[0] = static_cast<std::uint8_t>(v & 0xff);
    p[1] = static_cast<std::uint8_t>((static_cast<unsigned>(v) >> 8) & 0xff);
}
void wr32(std::uint8_t* p, std::uint32_t v) {
    wr16(p, static_cast<int>(v & 0xffff));
    wr16(p + 2, static_cast<int>(v >> 16));
}

}  // namespace

const char* extension(Kind k) {
    switch (k) {
    case Kind::kSav: return ".sav";  // DGROUP:0xfcf
    case Kind::kDon: return ".don";  // DGROUP:0xfc5
    default: return ".ded";          // DGROUP:0xfca
    }
}

std::size_t round_block(std::size_t n, Granularity g) {
    switch (g) {
    case Granularity::kOdd16: {
        // Standard mode: an odd multiple of 16 [REF Microsoft KB Q92942].
        std::size_t m = (n + 15) / 16;
        if (m == 0) m = 1;
        if (m % 2 == 0) ++m;
        return m * 16;
    }
    case Granularity::k64: return (n + 63) & ~static_cast<std::size_t>(63);
    case Granularity::k128: return (n + 127) & ~static_cast<std::size_t>(127);
    case Granularity::k256: return (n + 255) & ~static_cast<std::size_t>(255);
    default: return (n + 31) & ~static_cast<std::size_t>(31);
    }
}

const char* granularity_name(Granularity g) {
    switch (g) {
    case Granularity::kOdd16: return "odd16";
    case Granularity::k64: return "64";
    case Granularity::k128: return "128";
    case Granularity::k256: return "256";
    default: return "32";
    }
}

std::string Header::pilot_name() const {
    std::size_t n = 0;
    while (n < pilot.size() && pilot[n] != 0) ++n;
    return std::string(reinterpret_cast<const char*>(pilot.data()), n);
}

void Header::set_pilot(const std::string& s) {
    pilot.fill(0);
    const std::size_t n = std::min(s.size(), kPilotSize - 1);
    for (std::size_t i = 0; i < n; ++i) {
        if (s[i] == 0) break;
        pilot[i] = static_cast<std::uint8_t>(s[i]);
    }
}

std::array<std::uint8_t, kHeaderSize> encode_header(const Header& h) {
    HeaderBytes b{};  // GMEM_ZEROINIT [BYTES seg6:0x8a86]
    std::uint8_t* p = b.data();
    wr32(p + 0x00, h.game_id);
    std::memcpy(p + 0x04, h.pilot.data(), kPilotSize);
    p[0x18] = h.start_mon;
    p[0x19] = h.start_mday;
    p[0x1a] = h.start_year;
    p[0x1b] = h.save_mon;
    p[0x1c] = h.save_mday;
    p[0x1d] = h.save_year;
    wr32(p + 0x1e, h.score);
    wr16(p + 0x22, h.ship);
    wr16(p + 0x24, h.wave);
    wr16(p + 0x26, h.b5d);
    wr16(p + 0x28, h.b5e);
    wr16(p + 0x2a, h.record);
    wr16(p + 0x2c, h.player_roster);
    wr16(p + 0x2e, h.w9432);
    wr16(p + 0x30, h.w964a);
    wr16(p + 0x32, h.dir_count);
    wr16(p + 0x34, h.roster_last);
    for (std::size_t i = 0; i < 64; ++i) wr16(p + 0x36 + 2 * i, h.table[i]);
    for (std::size_t i = 0; i < 24; ++i) {
        wr16(p + 0xb6 + 2 * i, h.w4eec[i]);
        wr16(p + 0xe6 + 2 * i, h.w4fd0[i]);
        wr16(p + 0x116 + 2 * i, h.w6b10[i]);
    }
    for (std::size_t i = 0; i < 8; ++i) wr16(p + 0x146 + 2 * i, h.w6b40[i]);
    wr16(p + 0x156, h.w945c);
    wr16(p + 0x158, h.w945e);
    wr16(p + 0x15a, h.captured);
    wr16(p + 0x15c, h.destroyed);
    wr16(p + 0x15e, h.shape);
    wr16(p + 0x160, h.completed);
    wr16(p + 0x162, h.credit10);
    wr16(p + 0x164, h.resources);
    wr16(p + 0x166, h.w93ac);
    wr16(p + 0x168, h.w94ca);
    wr32(p + 0x16a, h.rating);
    p[0x16e] = h.play_mode;
    p[0x16f] = h.drive;
    p[0x170] = h.preview;
    p[0x171] = h.size;
    p[0x172] = h.weapon_slot;
    p[0x173] = h.w1318;
    for (std::size_t k = 0; k < 10; ++k) {
        p[0x174 + k] = h.a14d[k];
        p[0x17e + k] = h.a157[k];
    }
    p[0x188] = h.a161;
    p[0x189] = h.a162;
    p[0x18a] = h.w967c;
    wr32(p + 0x18b, h.frames);
    p[0x18f] = h.w9346;
    p[0x190] = h.a1cd_q;
    p[0x191] = h.a1cd_r;
    for (std::size_t k = 0; k < 4; ++k) p[0x192 + k] = h.a193[k];
    p[0x196] = h.a1cf;
    for (std::size_t k = 0; k < h.tail.size(); ++k) p[0x197 + k] = h.tail[k];
    return b;
}

Header decode_header(const std::uint8_t* p) {
    Header h;
    h.game_id = rd32(p + 0x00);
    std::memcpy(h.pilot.data(), p + 0x04, kPilotSize);
    h.start_mon = p[0x18];
    h.start_mday = p[0x19];
    h.start_year = p[0x1a];
    h.save_mon = p[0x1b];
    h.save_mday = p[0x1c];
    h.save_year = p[0x1d];
    h.score = rd32(p + 0x1e);
    h.ship = rds16(p + 0x22);
    h.wave = rds16(p + 0x24);
    h.b5d = rds16(p + 0x26);
    h.b5e = rds16(p + 0x28);
    h.record = rds16(p + 0x2a);
    h.player_roster = rds16(p + 0x2c);
    h.w9432 = rds16(p + 0x2e);
    h.w964a = rds16(p + 0x30);
    h.dir_count = rds16(p + 0x32);
    h.roster_last = rds16(p + 0x34);
    for (std::size_t i = 0; i < 64; ++i) h.table[i] = rds16(p + 0x36 + 2 * i);
    for (std::size_t i = 0; i < 24; ++i) {
        h.w4eec[i] = rds16(p + 0xb6 + 2 * i);
        h.w4fd0[i] = rds16(p + 0xe6 + 2 * i);
        h.w6b10[i] = rds16(p + 0x116 + 2 * i);
    }
    for (std::size_t i = 0; i < 8; ++i) h.w6b40[i] = rds16(p + 0x146 + 2 * i);
    h.w945c = rds16(p + 0x156);
    h.w945e = rds16(p + 0x158);
    h.captured = rds16(p + 0x15a);
    h.destroyed = rds16(p + 0x15c);
    h.shape = rds16(p + 0x15e);
    h.completed = rds16(p + 0x160);
    h.credit10 = rds16(p + 0x162);
    h.resources = rds16(p + 0x164);
    h.w93ac = rds16(p + 0x166);
    h.w94ca = rds16(p + 0x168);
    h.rating = rd32(p + 0x16a);
    h.play_mode = p[0x16e];
    h.drive = p[0x16f];
    h.preview = p[0x170];
    h.size = p[0x171];
    h.weapon_slot = p[0x172];
    h.w1318 = p[0x173];
    for (std::size_t k = 0; k < 10; ++k) {
        h.a14d[k] = p[0x174 + k];
        h.a157[k] = p[0x17e + k];
    }
    h.a161 = p[0x188];
    h.a162 = p[0x189];
    h.w967c = p[0x18a];
    h.frames = rd32(p + 0x18b);
    h.w9346 = p[0x18f];
    h.a1cd_q = p[0x190];
    h.a1cd_r = p[0x191];
    for (std::size_t k = 0; k < 4; ++k) h.a193[k] = p[0x192 + k];
    h.a1cf = p[0x196];
    for (std::size_t k = 0; k < h.tail.size(); ++k) h.tail[k] = p[0x197 + k];
    return h;
}

std::int16_t credit_word(std::int32_t* credit) {
    // Signed 32-bit compare: jl/jg on the high word, jbe on the low [BYTES seg6:0x8fe5-0x8ff4].
    if (*credit > 0x4fff6) *credit = 0x4fff6;  // written back [BYTES seg6:0x8fff]
    // seg5:0x110, the signed long divide (toward zero) [BYTES seg6:0x9009].
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(*credit / 10));
}

std::int16_t shape_of(const std::array<std::int16_t, 4>& systems, std::int16_t design_special) {
    const int n = design_special == -1 ? 3 : 4;  // [BYTES seg6:0x9137 seg6:0x913d]
    std::int16_t sum = 0;
    for (int i = 0; i < n; ++i)  // 16-bit add of ship words +0x26 + 2i
        sum = static_cast<std::int16_t>(static_cast<std::uint16_t>(sum) +
                                         static_cast<std::uint16_t>(systems[static_cast<std::size_t>(i)]));
    const std::int16_t a = static_cast<std::int16_t>(sum / n);  // cwd; idiv [BYTES seg6:0x917a]
    if (a > 70) return 100;                                     // jle [BYTES seg6:0x9180]
    if (a > 45) return 101;                                     // [BYTES seg6:0x9191]
    if (a > 27) return 102;                                     // [BYTES seg6:0x91a2]
    return 103;
}

std::uint8_t preview_flags(std::uint8_t has_flame, std::uint8_t b_ac, std::uint8_t b_ad) {
    if (has_flame == 0) return 0;  // [BYTES seg6:0x8efb seg6:0x8f07]
    return static_cast<std::uint8_t>((b_ac == 0 ? 1 : 0) + 2 * (b_ad == 0 ? 1 : 0));  // [BYTES seg6:0x8f0e-0x8f2d]
}

void split_a1cd(std::int16_t v, std::uint8_t* q, std::uint8_t* r) {
    // cwd; and dx, 0x7f; add ax, dx; sar ax, 7: toward zero [BYTES seg6:0x925d-0x9266]
    const std::int16_t qq = static_cast<std::int16_t>(v / 128);
    const std::int16_t rr = static_cast<std::int16_t>(v % 128);  // idiv's remainder [BYTES seg6:0x926e-0x9274]
    *q = static_cast<std::uint8_t>(qq & 0xff);
    *r = static_cast<std::uint8_t>(rr & 0xff);
}

std::int16_t join_a1cd(std::uint8_t q, std::uint8_t r) {
    // cbw; shl ax, 7 of +0x190 plus the cbw of +0x191 [BYTES seg6:0x9809-0x981b]
    const std::uint16_t hi = static_cast<std::uint16_t>(static_cast<std::uint16_t>(sx(q)) << 7);
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(hi + static_cast<std::uint16_t>(sx(r))));
}

std::uint16_t date_word(std::uint8_t mon, std::uint8_t mday) {
    // dh = +0x18, dl = 0, OR the cbw of +0x19 [BYTES seg6:0x9610-0x9621]
    return static_cast<std::uint16_t>((mon << 8) | static_cast<std::uint16_t>(sx(mday)));
}

std::optional<Parsed> parse(const std::vector<std::uint8_t>& file, Kind kind, std::string* why) {
    auto fail = [why](const std::string& s) -> std::optional<Parsed> {
        if (why) *why = s;
        return std::nullopt;
    };
    if (file.size() < kHeaderSize) return fail("shorter than the 0x1a9-byte header");
    const std::uint8_t* p = file.data();
    const std::int16_t last = rds16(p + 0x34);
    const std::int16_t dirs = rds16(p + 0x32);
    const std::int16_t record = rds16(p + 0x2a);
    if (last < 0 || last > kMaxRosterLast) return fail("header +0x34 outside 0 ... 254");
    const bool sav = kind == Kind::kSav;
    if (sav) {
        if (dirs < 1 || dirs > kMaxDirRecords) return fail("header +0x32 outside 1 ... 98");
        // ADR 0014 decision 9: seg6:0x9645-0x9655 indexes the block with +0x2a unchecked.
        if (record < 0 || record >= dirs) return fail("header +0x2a outside 0 ... +0x32 - 1");
    }
    // ADR 0014 decision 9: the reader's lstrcpy of +0x04 [BYTES seg6:0x9764] needs its NUL in
    // the field.
    bool nul = false;
    for (std::size_t i = 0; i < kPilotSize; ++i)
        if (p[0x04 + i] == 0) nul = true;
    if (!nul) return fail("no NUL in the pilot name +0x04 ... +0x17");

    const std::size_t roster_n = (static_cast<std::size_t>(last) + 1) * kRosterRecordSize;
    const std::size_t dir_n = sav ? static_cast<std::size_t>(dirs) * kDirRecordSize : 0;
    for (Granularity g : kGranularityOrder) {
        const std::size_t h = round_block(kHeaderSize, g);
        const std::size_t r = round_block(roster_n, g);
        const std::size_t d = sav ? round_block(dir_n, g) : 0;
        if (h + r + d != file.size()) continue;
        Parsed out;
        std::copy(p, p + kHeaderSize, out.header_bytes.begin());
        out.header = decode_header(p);
        out.granularity = g;
        out.roster.resize(static_cast<std::size_t>(last) + 1);
        for (std::size_t k = 0; k < out.roster.size(); ++k)
            std::copy(p + h + k * kRosterRecordSize, p + h + (k + 1) * kRosterRecordSize, out.roster[k].begin());
        if (sav) {
            out.dirs.resize(static_cast<std::size_t>(dirs));
            for (std::size_t k = 0; k < out.dirs.size(); ++k)
                std::copy(p + h + r + k * kDirRecordSize, p + h + r + (k + 1) * kDirRecordSize,
                          out.dirs[k].begin());
        }
        return out;
    }
    return fail("no granularity fits the length " + std::to_string(file.size()));
}

std::vector<std::uint8_t> build(const HeaderBytes& header, const std::vector<RosterBytes>& roster,
                                const std::vector<DirBytes>& dirs, Kind kind, Granularity g) {
    std::vector<std::uint8_t> out(header.begin(), header.end());
    out.resize(round_block(kHeaderSize, g), 0);
    std::size_t start = out.size();
    for (const RosterBytes& r : roster) out.insert(out.end(), r.begin(), r.end());
    out.resize(start + round_block(roster.size() * kRosterRecordSize, g), 0);
    if (kind == Kind::kSav) {  // [BYTES seg6:0x8b25]
        start = out.size();
        for (const DirBytes& d : dirs) out.insert(out.end(), d.begin(), d.end());
        out.resize(start + round_block(dirs.size() * kDirRecordSize, g), 0);
    }
    return out;
}

std::string base_name(bool named, Kind kind, const std::string& restore9, const std::string& pilot,
                      bool player_game) {
    if (!named) return player_game ? "lastgame" : "demogame";  // DGROUP:0xfb3, 0xfbc [BYTES seg6:0x88d2 seg6:0x88dd]
    std::string buf = restore9.substr(0, std::min<std::size_t>(restore9.size(), 8));  // 9 bytes [BYTES seg6:0x8906]
    if (kind != Kind::kSav && buf.size() > 3 && buf[3] == ' ') {  // [BYTES seg6:0x8911]
        std::string tmp = pilot.substr(0, std::min<std::size_t>(pilot.size(), 7));  // tmp[7] = 0 [BYTES seg6:0x8927]
        buf = tmp;
        const std::size_t sp = buf.find(' ');  // the shrinking lstrlen loop [BYTES seg6:0x8944-0x8964]
        if (sp != std::string::npos) buf.resize(sp);
    }
    const std::size_t nul = buf.find('\0');
    if (nul != std::string::npos) buf.resize(nul);
    return buf;
}

std::string retry_name(const std::string& base, int counter, Kind kind) {
    std::string b = base.substr(0, std::min<std::size_t>(base.size(), 5));  // bp-0x8d = 0 [BYTES seg6:0x8a06]
    return b + std::to_string(counter) + extension(kind);  // seg6:0xf1a [BYTES seg6:0x8a21 seg6:0x8a29]
}

int name_check(const std::vector<std::uint8_t>* head, std::size_t need, std::uint32_t game_id) {
    if (!head) return 1;                     // OpenFile failed [BYTES seg6:0x879c-0x87a1]
    if (head->size() < need) return 1;       // short read [BYTES seg6:0x8807 seg6:0x8811]
    if (head->size() < 4) return 1;
    return rd32(head->data()) == game_id ? 1 : 0;  // [BYTES seg6:0x8819-0x882f]
}

std::string free_name(const std::string& first, const std::string& base, Kind kind,
                      const std::function<int(const std::string&)>& check) {
    std::string name = first;
    int c = 0;  // [bp-0x188] [BYTES seg6:0x89cc]
    while (check(name) == 0) {
        if (c >= 0x7fff) break;  // [DESIGN]
        name = retry_name(base, ++c, kind);
    }
    return name;
}

bool purge_match(const std::vector<std::uint8_t>& head, std::uint32_t game_id) {
    return head.size() >= kScoreHead && rd32(head.data()) == game_id;
}

// ---- the score list

std::uint32_t ScoreEntry::id() const { return rd32(b.data()); }
void ScoreEntry::set_id(std::uint32_t v) { wr32(b.data(), v); }
std::int32_t ScoreEntry::score() const { return static_cast<std::int32_t>(rd32(b.data() + 0x1e)); }
void ScoreEntry::set_score(std::int32_t v) { wr32(b.data() + 0x1e, static_cast<std::uint32_t>(v)); }
std::int16_t ScoreEntry::ship() const { return rds16(b.data() + 0x22); }
void ScoreEntry::set_ship(std::int16_t v) { wr16(b.data() + 0x22, v); }
std::string ScoreEntry::pilot() const {
    std::size_t n = 0;
    while (n < kPilotSize && b[0x04 + n] != 0) ++n;
    return std::string(reinterpret_cast<const char*>(b.data() + 0x04), n);
}
std::string ScoreEntry::file() const {
    std::size_t n = 0;
    while (n < 10 && b[0x26 + n] != 0) ++n;
    return std::string(reinterpret_cast<const char*>(b.data() + 0x26), n);
}

namespace {
void put_text(ScoreEntry& e, std::size_t off, std::size_t room, const std::string& s) {
    for (std::size_t i = 0; i < room; ++i) e.b[off + i] = 0;
    const std::size_t n = std::min(s.size(), room - 1);  // bounded at the field [DESIGN ADR 0014 decision 9]
    for (std::size_t i = 0; i < n; ++i) e.b[off + i] = static_cast<std::uint8_t>(s[i]);
}
}  // namespace

ScoreEntry running_entry(const RunningGame& g) {
    ScoreEntry e;  // GlobalAlloc(0x42, 0x31) [BYTES seg6:0x65b2]
    e.set_score(g.score);
    e.set_id(g.game_id);
    e.set_ship(g.ship);
    wr16(e.b.data() + 0x24, g.wave);
    e.b[0x18] = static_cast<std::uint8_t>(g.date >> 8);
    e.b[0x19] = static_cast<std::uint8_t>(g.date & 0xff);
    e.b[0x1a] = g.year;
    e.b[0x1b] = g.now_mon;
    e.b[0x1c] = g.now_mday;
    e.b[0x1d] = g.now_year;
    put_text(e, 0x04, kPilotSize, g.pilot);
    put_text(e, 0x26, 10, "--");  // DGROUP:0xf5e
    e.b[0x30] = 0;
    return e;
}

ScoreList build_score_list(const RunningGame& g, const std::vector<ScoreFile>& files, int shp_count,
                           const std::function<bool(int)>& ship_ok) {
    ScoreList out;
    std::vector<ScoreEntry> t;
    t.push_back(running_entry(g));
    for (const ScoreFile& f : files) {
        // An open failure or a short read returns from seg6:0x6576 at once (jmp seg6:0x7039)
        // [BYTES seg6:0x6754 seg6:0x6779]; ADR 0014 decision 9 skips the file instead [DESIGN].
        if (!f.opened || f.head.size() < kScoreHead) {
            ++out.skipped;
            continue;
        }
        ScoreEntry e;
        std::copy(f.head.begin(), f.head.begin() + kScoreHead, e.b.begin());
        // The running game's own file whose score is not above it is dropped [BYTES seg6:0x678d
        // seg6:0x67a0 seg6:0x67b4].
        if (e.id() == g.game_id && e.score() <= g.score) e.set_id(0);
        // The found name without its last 4 characters [BYTES seg6:0x67c0-0x6800].
        std::string n = f.name;
        n.resize(n.size() >= 4 ? n.size() - 4 : 0);
        put_text(e, 0x26, 10, n);
        e.b[0x30] = static_cast<std::uint8_t>(f.pass);  // [BYTES seg6:0x680d]
        t.push_back(e);
    }
    const int n = static_cast<int>(t.size());
    // Step 4, the rank [BYTES seg6:0x686a-0x69b5].
    for (int p = 0; p < n; ++p) {
        std::int32_t best = -1;
        int j = -1;
        for (int e = 0; e < n; ++e) {
            const ScoreEntry& x = t[static_cast<std::size_t>(e)];
            if (x.id() != 0 && x.score() >= 0 && x.score() > best) {
                best = x.score();
                j = e;
            }
        }
        if (j == -1) break;
        if (j == 0) {
            out.rank = p + 1;  // DGROUP:0x93a2's only writer [BYTES seg6:0x68eb]
            out.rank_set = true;
        }
        const std::uint32_t id = t[static_cast<std::size_t>(j)].id();
        for (int e = j == 0 ? 0 : 1; e < n; ++e) {  // [BYTES seg6:0x6905-0x691b]
            ScoreEntry& x = t[static_cast<std::size_t>(e)];
            if (x.id() == id && x.score() >= 0) x.set_score(-1 - x.score());
        }
    }
    for (ScoreEntry& x : t)
        if (x.score() < 0) x.set_score(-1 - x.score());
    // Step 5, the order [BYTES seg6:0x69b7-0x6c6c].
    for (int p = 0; p < n; ++p) {
        std::int32_t best = -1;
        int j = -1;
        for (int e = 0; e < n; ++e) {
            const ScoreEntry& x = t[static_cast<std::size_t>(e)];
            if (x.id() != 0 && x.score() > best) {
                best = x.score();
                j = e;
            }
        }
        if (j == -1) break;
        ScoreEntry& pick = t[static_cast<std::size_t>(j)];
        // The ship reset [BYTES seg6:0x6ac2-0x6af3].
        if (pick.ship() > shp_count - 1 || (ship_ok && !ship_ok(pick.ship()))) pick.set_ship(0);
        out.list.push_back(pick);
        const std::uint32_t id = pick.id();
        for (int e = j == 0 ? 0 : 1; e < n; ++e) {  // [BYTES seg6:0x6bff-0x6c15]
            ScoreEntry& x = t[static_cast<std::size_t>(e)];
            if (x.id() == id) x.set_id(0);
        }
    }
    return out;
}

std::vector<TableSlot> champion_slots(const ScoreList& l, const std::function<std::string(int)>& ship_name) {
    std::vector<TableSlot> out;
    const int rank = l.rank;
    for (int i = 0; i < static_cast<int>(l.list.size()); ++i) {
        // [BYTES seg6:0x6d08-0x6d3e]
        const bool in = (rank < 8 && i < 10) || i < 3 || (rank - 5 < i && i < rank + 3);
        if (!in) continue;
        if (static_cast<int>(out.size()) >= kTableSlots) break;  // the 0x1cc block [DESIGN]
        const ScoreEntry& e = l.list[static_cast<std::size_t>(i)];
        TableSlot s;
        s.place = static_cast<std::int16_t>(i + 1);
        s.score = e.score();
        s.text = e.pilot() + " - " + (ship_name ? ship_name(e.ship()) : std::string());  // DGROUP:0xfa0
        out.push_back(s);
    }
    return out;
}

}  // namespace is::fmt::save
