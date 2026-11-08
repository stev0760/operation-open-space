// The saved-game files `.sav`, `.ded` and `.don` (docs/re/formats.md "Saved games", ADR 0014):
// the 0x1a9-byte header of seg6:0x8eea / seg6:0x930a field by field, the roster block of 0x7c-byte
// records, the directory block of 0x297-byte records, the block lengths rounded to a host's
// GlobalSize granularity (ADR 0014 decision 1), the reader's validation, the names of the writer
// seg6:0x885c (steps 3 to 5), the purge seg6:0x7042's and the name check seg6:0x8776's tests,
// and the score list seg6:0x6576. Portable: no file I/O, the callers hand bytes in and out.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace is::fmt::save {

constexpr std::size_t kHeaderSize = 0x1a9;        // GlobalAlloc(0x42, 0x1a9) [BYTES seg6:0x8a86 seg6:0x93e0]
constexpr std::size_t kRosterRecordSize = 0x7c;   // [BYTES seg6:0x9492]
constexpr std::size_t kDirRecordSize = 0x297;     // [BYTES seg6:0x952a]
constexpr std::size_t kPilotSize = 20;            // +0x04 ... +0x17
constexpr std::size_t kScoreHead = 0x26;          // the bytes seg6:0x6576 and seg6:0x7042 read [BYTES seg6:0x6759 seg6:0x70f4]
// ADR 0014 decision 1, validation: +0x34 is 0 ... 254 (a roster index is a byte, 0xff free) and
// a .sav's +0x32 is 1 ... 98 (the records a 64 KB block holds) [DESIGN ADR 0014 decision 1].
constexpr int kMaxRosterLast = 254;
constexpr int kMaxDirRecords = 98;

using RosterBytes = std::array<std::uint8_t, kRosterRecordSize>;
using DirBytes = std::array<std::uint8_t, kDirRecordSize>;
using HeaderBytes = std::array<std::uint8_t, kHeaderSize>;

// seg6:0x885c's kind: 0 .sav (Games), 1 .ded and 2 .don (Dead) [BYTES seg6:0x8993-0x89be].
enum class Kind { kSav = 0, kDed = 1, kDon = 2 };
// ".sav" DGROUP:0xfcf, ".ded" DGROUP:0xfca, ".don" DGROUP:0xfc5 [BYTES seg6:0x8993-0x89be].
const char* extension(Kind k);

// ---- block lengths (ADR 0014 decision 1)
// GlobalSize's rounding of an allocation: Wine's krnl386 and Windows 3.1 enhanced mode round
// up to a multiple of 32 [REF Wine 11.0 dlls/krnl386.exe16/global.c GLOBAL_Alloc] [REF
// Microsoft KB Q92942]; standard mode gives an odd multiple of 16 (16, 48, 80 ...) [REF KB
// Q92942]; 64, 128 and 256 are the other candidates for the Windows 98 machine [OPEN].
enum class Granularity : std::uint8_t { k32 = 0, kOdd16, k64, k128, k256 };
// The reader's fixed order: 32, standard mode's odd 16s, 64, 128, 256 [DESIGN ADR 0014 d1].
constexpr std::array<Granularity, 5> kGranularityOrder = {
    Granularity::k32, Granularity::kOdd16, Granularity::k64, Granularity::k128, Granularity::k256};
// A new game's granularity [DESIGN ADR 0014 decision 1].
constexpr Granularity kNewGameGranularity = Granularity::k32;
std::size_t round_block(std::size_t n, Granularity g);
const char* granularity_name(Granularity g);

// ---- the header, every field at the original's offset and width (formats.md "Header")
// Byte fields hold the stored byte; the reader's sign extension is install's (decode_* below).
struct Header {
    std::uint32_t game_id = 0;                     // +0x00 DGROUP:0x939a/0x939c [BYTES seg6:0x90aa-0x90b7]
    std::array<std::uint8_t, kPilotSize> pilot{};  // +0x04 DGROUP:0x380c, NUL-terminated [BYTES seg6:0x912c]
    std::uint8_t start_mon = 0;                    // +0x18 high byte of DGROUP:0x939e, tm_mon [BYTES seg6:0x90c1]
    std::uint8_t start_mday = 0;                   // +0x19 low byte of DGROUP:0x939e [BYTES seg6:0x90c8]
    std::uint8_t start_year = 0;                   // +0x1a low byte of DGROUP:0x93a0, tm_year [BYTES seg6:0x90cf]
    std::uint8_t save_mon = 0, save_mday = 0, save_year = 0;  // +0x1b..+0x1d seg10:0x804c(0) [BYTES seg6:0x90d3-0x90ee]
    std::uint32_t score = 0;                       // +0x1e DGROUP:0x1e58 [BYTES seg6:0x8f8c-0x8f97]
    std::int16_t ship = 0;                         // +0x22 roster[DGROUP:0x931c] word +0x00 [BYTES seg6:0x9034-0x9043]
    std::int16_t wave = 0;                         // +0x24 DGROUP:0x91f8 [BYTES seg6:0x8f5e]
    std::int16_t b5d = 0;                          // +0x26 (int8) player byte +0x5d [BYTES seg6:0x8f43-0x8f48]
    std::int16_t b5e = 0;                          // +0x28 (int8) player byte +0x5e [BYTES seg6:0x8f52-0x8f57]
    std::int16_t record = 0;                       // +0x2a DGROUP:0x94a2 [BYTES seg6:0x8f65]
    std::int16_t player_roster = 0;                // +0x2c DGROUP:0x931c [BYTES seg6:0x8f6c]
    std::int16_t w9432 = 0;                        // +0x2e DGROUP:0x9432 [BYTES seg6:0x8f73]
    std::int16_t w964a = 0;                        // +0x30 DGROUP:0x964a [BYTES seg6:0x8f7a]
    std::int16_t dir_count = 0;                    // +0x32 DGROUP:0x94a4 [BYTES seg6:0x8f81]
    std::int16_t roster_last = 0;                  // +0x34 DGROUP:0x931a [BYTES seg6:0x8f88]
    std::array<std::int16_t, 64> table{};          // +0x36 DGROUP:0x3824, 8 x 8 [BYTES seg6:0x91c7-0x91dd]
    std::array<std::int16_t, 24> w4eec{};          // +0xb6 DGROUP:0x4eec, 8 x 3 [BYTES seg6:0x91fb-0x9206]
    std::array<std::int16_t, 24> w4fd0{};          // +0xe6 DGROUP:0x4fd0 [BYTES seg6:0x920b-0x920f]
    std::array<std::int16_t, 24> w6b10{};          // +0x116 DGROUP:0x6b10 [BYTES seg6:0x9214-0x9218]
    std::array<std::int16_t, 8> w6b40{};           // +0x146 DGROUP:0x6b40 [BYTES seg6:0x922b-0x9236]
    std::int16_t w945c = 0;                        // +0x156 DGROUP:0x945c [BYTES seg6:0x924d]
    std::int16_t w945e = 0;                        // +0x158 DGROUP:0x945e [BYTES seg6:0x9255]
    std::int16_t captured = 0;                     // +0x15a player word +0xd2 [BYTES seg6:0x8fcc-0x8fd6]
    std::int16_t destroyed = 0;                    // +0x15c player word +0xd0 [BYTES seg6:0x8fbd-0x8fc7]
    std::int16_t shape = 0;                        // +0x15e 100 ... 103 (shape_of) [BYTES seg6:0x9131-0x91b3]
    std::int16_t completed = 0;                    // +0x160 records with +0x1d0 = 2 [BYTES seg6:0x90f2-0x911b]
    std::int16_t credit10 = 0;                     // +0x162 capped credit / 10 (credit_word) [BYTES seg6:0x8fdb-0x9011]
    std::int16_t resources = 0;                    // +0x164 player word +0xce [BYTES seg6:0x9016-0x9020]
    std::int16_t w93ac = 0;                        // +0x166 DGROUP:0x93ac [BYTES seg6:0x9061]
    std::int16_t w94ca = 0;                        // +0x168 DGROUP:0x94ca [BYTES seg6:0x9069]
    std::uint32_t rating = 0;                      // +0x16a DGROUP:0x5078 [BYTES seg6:0x8f9b-0x8fa7]
    std::uint8_t play_mode = 0;                    // +0x16e low byte of DGROUP:0x9134 [BYTES seg6:0x904e-0x9051]
    std::uint8_t drive = 0;                        // +0x16f low byte of DGROUP:0x94ac [BYTES seg6:0x9056-0x9059]
    std::uint8_t preview = 0;                      // +0x170 preview_flags [BYTES seg6:0x8efb-0x8f2d]
    std::uint8_t size = 0;                         // +0x171 low byte of DGROUP:0x9156 [BYTES seg6:0x8f32-0x8f38]
    std::uint8_t weapon_slot = 0;                  // +0x172 low byte of player word +0x18 [BYTES seg6:0x902b-0x902f]
    std::uint8_t w1318 = 0;                        // +0x173 low byte of DGROUP:0x1318 [BYTES seg6:0x92a4-0x92aa]
    std::array<std::uint8_t, 10> a14d{};           // +0x174 DGROUP:0xa14d [BYTES seg6:0x9086-0x9090]
    std::array<std::uint8_t, 10> a157{};           // +0x17e DGROUP:0xa157 [BYTES seg6:0x9095-0x909c]
    // +0x188, +0x189: never written (0 from GMEM_ZEROINIT); the reader loads them into
    // DGROUP:0xa161 and DGROUP:0xa162 [BYTES seg6:0x96f7-0x9704]. Kept as read.
    std::uint8_t a161 = 0, a162 = 0;
    std::uint8_t w967c = 0;                        // +0x18a low byte of DGROUP:0x967c [BYTES seg6:0x9076-0x9079]
    std::uint32_t frames = 0;                      // +0x18b DGROUP:0x5a8, unaligned [BYTES seg6:0x8fac-0x8fb8]
    std::uint8_t w9346 = 0;                        // +0x18f low byte of DGROUP:0x9346 [BYTES seg6:0x906e-0x9071]
    std::uint8_t a1cd_q = 0;                       // +0x190 DGROUP:0xa1cd / 128 (split_a1cd) [BYTES seg6:0x925a-0x9266]
    std::uint8_t a1cd_r = 0;                       // +0x191 DGROUP:0xa1cd % 128 [BYTES seg6:0x926b-0x9274]
    std::array<std::uint8_t, 4> a193{};            // +0x192 DGROUP:0xa193 ... 0xa196 [BYTES seg6:0x9281-0x92a2]
    std::uint8_t a1cf = 0;                         // +0x196 low byte of DGROUP:0xa1cf [BYTES seg6:0x9279-0x927c]
    // +0x197 ... +0x1a8: never written, 0 (GMEM_ZEROINIT), never read [BYTES seg6:0x8eea-0x92b6].
    std::array<std::uint8_t, 18> tail{};

    // The pilot up to its NUL (at most 20 bytes).
    std::string pilot_name() const;
    // lstrcpy(H + 4, pilot) bounded at the field: at most 19 bytes and a NUL [DESIGN ADR 0014
    // decision 9: the original's copy is unbounded; INI ThePlayer is read into 20 bytes, so it
    // fits].
    void set_pilot(const std::string& s);
};

std::array<std::uint8_t, kHeaderSize> encode_header(const Header& h);
// `p` holds at least kHeaderSize bytes.
Header decode_header(const std::uint8_t* p);

// ---- the computed fields and the reader's decodings
// +0x162 (seg6:0x8fdb-0x9011): the credit dword capped at 0x4fff6 (signed compare, the cap is
// written back into the ship record: *credit is updated), then / 10 by seg5:0x110 (signed,
// toward zero), stored as a word.
std::int16_t credit_word(std::int32_t* credit);
// +0x15e (seg6:0x9131-0x91b3): n = 3 when the design word +0x28 is -1, else 4; the 16-bit sum
// of the first n system words, idiv n; > 70 → 100, > 45 → 101, > 27 → 102, else 103.
std::int16_t shape_of(const std::array<std::int16_t, 4>& systems, std::int16_t design_special);
// +0x170 (seg6:0x8efb-0x8f2d): 0, or with byte +0xe4 != 0 (byte +0xac = 0) + 2 (byte +0xad = 0).
std::uint8_t preview_flags(std::uint8_t has_flame, std::uint8_t b_ac, std::uint8_t b_ad);
// +0x190/+0x191 (seg6:0x925a-0x9274): q = v / 128 toward zero (cwd; and dx,0x7f; add; sar 7),
// r = the idiv remainder (the dividend's sign); each stored as its low byte.
void split_a1cd(std::int16_t v, std::uint8_t* q, std::uint8_t* r);
// The reader (seg6:0x9809-0x981b): (int8)q << 7 + (int8)r, 16-bit. Exact for -16511 ... 16383.
std::int16_t join_a1cd(std::uint8_t q, std::uint8_t r);
// DGROUP:0x939e = (+0x18 << 8) | (int16)(int8)+0x19: a day byte >= 0x80 sets the high byte to
// 0xff (seg6:0x9610-0x9621).
std::uint16_t date_word(std::uint8_t mon, std::uint8_t mday);
// The sign extension of a byte field (cbw) [BYTES seg6:0x972c seg6:0x9746 seg6:0x970c ...].
inline std::int16_t sx(std::uint8_t b) { return static_cast<std::int8_t>(b); }

// ---- reading (ADR 0014 decisions 1 and 9)
struct Parsed {
    HeaderBytes header_bytes{};
    Header header;
    std::vector<RosterBytes> roster;  // header +0x34 + 1 records
    std::vector<DirBytes> dirs;       // header +0x32 records (.sav only)
    Granularity granularity = Granularity::k32;
};
// The nominal sizes from the header, the granularity search in kGranularityOrder (the first
// whose rounded lengths sum to the file's length) and the validation: at least 0x1a9 bytes;
// +0x34 within 0 ... 254; for a .sav +0x32 within 1 ... 98 and 0 <= +0x2a < +0x32 (decision 9);
// the pilot's NUL within +0x04 ... +0x17 (decision 9). Null with `why` set when refused.
std::optional<Parsed> parse(const std::vector<std::uint8_t>& file, Kind kind, std::string* why = nullptr);

// ---- writing (ADR 0014 decision 1)
// The header, then each block at its nominal size rounded to `g` and zero-padded; the roster
// always, the directory records for a .sav only [BYTES seg6:0x8add-0x8b48]. seg6:0x9e66's
// 0x7fff-byte chunks are a 16-bit _lwrite limit with no trace in the file: the port writes the
// bytes whole [DESIGN ADR 0014 decision 1, "Writing is atomic"; decision 9] (triage D-37).
std::vector<std::uint8_t> build(const HeaderBytes& header, const std::vector<RosterBytes>& roster,
                                const std::vector<DirBytes>& dirs, Kind kind, Granularity g);

// ---- the writer's names (seg6:0x885c steps 4 and 5)
// The base name: named = 0 → "lastgame" (DGROUP:0xfb3), or "demogame" (DGROUP:0xfbc) when
// player_game = 0; named != 0 → INI Restore as read into 9 bytes, and for kind != 0 with
// Restore[3] = ' ' the pilot cut at 7 characters and then at its first space [BYTES
// seg6:0x88ee-0x8964] (verify claim 14: only on the named branch).
std::string base_name(bool named, Kind kind, const std::string& restore9, const std::string& pilot,
                      bool player_game);
// The free-name loop's retry c (from 1): the base cut to 5 characters in place (bp-0x8d), the
// decimal counter (seg6:0xf1a), ".ded" or ".don" (DGROUP:0xfd4/0xfd9) [BYTES seg6:0x89cc-0x8a58].
std::string retry_name(const std::string& base, int counter, Kind kind);
// seg6:0x8776(name), corrected claim 16: 1 when the file does not open (null `head`), when it
// gives fewer than `need` bytes (GlobalSize of the 0x1a9 block) or when its dword +0x00 equals
// the game id; 0 only for a readable file of another game [BYTES seg6:0x879c-0x87a1
// seg6:0x8807 seg6:0x8811 seg6:0x8819-0x882f].
int name_check(const std::vector<std::uint8_t>* head, std::size_t need, std::uint32_t game_id);
// The loop: test `first`; while the check gives 0, try retry_name(base, ++c). `check` maps a
// candidate file name to seg6:0x8776's result. The counter stops at 0x7fff, the signed word
// itoa prints, with the last candidate returned [DESIGN].
std::string free_name(const std::string& first, const std::string& base, Kind kind,
                      const std::function<int(const std::string&)>& check);
// seg6:0x7042's test: a full 0x26-byte read whose dword +0x00 equals the game id [BYTES
// seg6:0x70f4 seg6:0x7127-0x7149].
bool purge_match(const std::vector<std::uint8_t>& head, std::uint32_t game_id);

// ---- the score list seg6:0x6576 (formats.md "Score list")
constexpr std::size_t kScoreEntrySize = 0x31;  // [BYTES seg6:0x6718]
struct ScoreEntry {
    std::array<std::uint8_t, kScoreEntrySize> b{};
    std::uint32_t id() const;
    void set_id(std::uint32_t v);
    std::int32_t score() const;          // +0x1e
    void set_score(std::int32_t v);
    std::int16_t ship() const;           // +0x22
    void set_ship(std::int16_t v);
    std::string pilot() const;           // +0x04
    std::string file() const;            // +0x26, 10 bytes
    std::uint8_t status() const { return b[0x30]; }  // 0 .sav, 1 .ded, 2 .don
};
// One file the scan found: the bytes read (fewer than 0x26 when short), whether it opened, the
// found name and the pass k (0 Games *.sav, 1 Dead *.ded, 2 Dead *.don).
struct ScoreFile {
    bool opened = false;
    std::vector<std::uint8_t> head;
    std::string name;
    int pass = 0;
};
// The running game's entry T[0] (seg6:0x6597-0x6678).
struct RunningGame {
    std::int32_t score = 0;
    std::uint32_t game_id = 0;
    std::int16_t ship = 0;
    std::int16_t wave = 0;
    std::uint16_t date = 0;      // DGROUP:0x939e
    std::uint8_t year = 0;       // DGROUP:0x93a0 low byte
    std::uint8_t now_mon = 0, now_mday = 0, now_year = 0;  // seg10:0x804c(0)
    std::string pilot;
};
ScoreEntry running_entry(const RunningGame& g);
struct ScoreList {
    std::vector<ScoreEntry> list;  // DGROUP:0x6af8, count DGROUP:0x94c6
    int rank = 0;                  // DGROUP:0x93a2 (0: not set by this list)
    bool rank_set = false;
    int skipped = 0;               // files skipped by decision 9
};
// Steps 3 to 5: each file appended (an unopenable or short file is skipped, ADR 0014 decision
// 9, where the original gives up the whole list [DESIGN ADR 0014 decision 9]); a file of the running game whose
// score is not above it is dropped; the rank pass; the order pass with the ship reset (above
// shp_count - 1, signed, or refused by `ship_ok`, seg14:0x455c) [BYTES seg6:0x6680-0x6c6c].
ScoreList build_score_list(const RunningGame& g, const std::vector<ScoreFile>& files, int shp_count,
                           const std::function<bool(int)>& ship_ok);

// Step 6, state 5: the champions table, at most 10 slots of 0x2e bytes (seg6:0x6d08-0x6dcd).
struct TableSlot {
    std::string text;      // +0x00 pilot + " - " (DGROUP:0xfa0) + ship name
    std::int32_t score = 0;  // +0x28
    std::int16_t place = 0;  // +0x2c i + 1
};
constexpr int kTableSlots = 10;  // 0x1cc / 0x2e [BYTES seg6:0x5ddc]
// Entry i is a slot when (rank < 8 and i < 10) or i < 3 or rank - 5 < i < rank + 3.
std::vector<TableSlot> champion_slots(const ScoreList& l, const std::function<std::string(int)>& ship_name);

}  // namespace is::fmt::save
