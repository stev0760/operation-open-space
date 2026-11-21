// The activity text of a ship and the object names it uses (docs/re/shell.md "The activity text
// seg3:0x1d0a", "The articles of seg3:0x1542"; work/analysis/m5/s4-commands.md 3.6 with
// s4-commands-verify.md claims 66-69 and its Omissions; M5 stage 4, track WATCH):
//  - seg3:0x1542(caps, pool, rec, buf, mode), the object name with its article;
//  - seg11:0x3fd4(text, mode), the casing helper;
//  - seg3:0x1d0a(a, s, mode), what ship s is doing and why, as seen by a;
//  - seg3:0x304c(a, k, mode), a defender's text;
//  - seg3:0x31d0(mode, l1, l2), the two lines' punctuation and where they go.
// The routines read the World (the ship records, the pools, the icons, the pool-8 side table)
// directly; the strings, the think call and the words the World does not hold come through
// ActivityHost. LoadString is modelled as Win16's: a 40-byte buffer (39 characters), and a
// missing string returns 0 and leaves the buffer as it was. Portable, no Win32.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "briefing/lang_format.h"

namespace is::world {
class World;
}

namespace is::hud {

// seg11:0x3fd4(text, mode): briefing::case_3fd4 (lang_format.h).
using briefing::case_3fd4;

// What the names and the texts read beyond the World. Every member names the original's word or
// routine; the defaults stand for a program without that part.
class ActivityHost {
public:
    virtual ~ActivityHost() = default;
    // LoadString(DGROUP:0x9212, id, buf, cap): the string cut to cap - 1 characters; false, with
    // `buf` untouched, when ENG_LANG.DLL has no string `id` (Win16 LoadString returns 0) [DESIGN].
    virtual bool load(int id, std::string& buf, int cap = 0x28) = 0;
    // A string for the formatter (empty when missing).
    std::string lang(int id) {
        std::string s;
        load(id, s, 0x50);
        return s;
    }
    // DGROUP:0x1e50, the clock.
    virtual std::uint32_t now() const { return 0; }
    // Design byte +0x44 (DGROUP:0x9716 + 0xad·n), bit 0x20 of the body's byte +9 (seg14:0x1efd-0x1f09).
    virtual int design_flags44(int /*design*/) const { return 0; }
    // DGROUP:0x967a: 1 when string 2902 starts with '2' [BYTES seg6:0x17ad-0x17cc].
    virtual int w967a() const { return 0; }
    // DGROUP:0x9460, the help window (M8): 0 in the wave and the briefing.
    virtual int w9460() const { return 0; }
    // The weapon of missile entry rec (DGROUP:0x66e4 + 0x1c·rec) and the weapon table's name word
    // (DGROUP:0x2f54 + 0x50·w) for seg3:0x1542's pool 6.
    virtual int missile_weapon(int /*rec*/) const { return 0; }
    virtual int weapon_name(int /*w*/) const { return -1; }
    // seg4:0x78(s), the think, which the demo's give-up of seg3:0x1d0a calls.
    virtual void think(int /*s*/) {}
    // An unported or undefined path, logged once per address by the host.
    virtual void todo(const std::string& /*what*/, const char* /*address*/) {}
};

// seg3:0x1542(caps, pool, rec, buf, mode) [BYTES seg3:0x1542-0x1ae4]: `buf` is the caller's
// buffer, which pools 5 and 8 leave as it was for an object without a name. The ship pool is the
// literal 14 (pitfalls: a literal 14).
std::string object_name(const world::World& w, ActivityHost& h, int caps, int pool, int rec, int mode,
                        std::string buf = {});
// seg17:0x8e60(i, buf) [BYTES seg17:0x8e60-0x8f1c]: a ship with a roster record (+0xd6 >= 0) is
// named by its record's string +0x65; an Enforcer (no record) is string 788 "Officer", " " and
// lo16(2·lo16(lo16(lo16((2i + 3)·(c + 2))·(i + 6))·(i + 7))) / (c + 3) + 956, c = sbyte +0x5e
// (16-bit `imul` low words, CWD, `idiv`) [BYTES seg17:0x8ebd-0x8ee5]. The same reading as
// src/hud/hooks.cpp ship_name_8e60, made portable here; a divisor c + 3 = 0 gives 0 [DESIGN ADR
// 0016].
std::string pilot_name_8e60(const world::World& w, ActivityHost& h, int i);
// seg3:0x1ae6(buf): the gender of a name from its code letter (^M 0, ^F 2, ^P 4, ^N 6) and the
// code cut; 6 and no cut without a code [BYTES seg3:0x1ae6-0x1b89].
int name_gender(std::string& buf);

// The DGROUP buffers the text routines write (one set per program, as DGROUP is).
struct ActivityState {
    std::string w8630;                 // DGROUP:0x8630, modes 0 and 1 (the Strategy command line)
    std::string w8db0, w8768, w87b8;   // DGROUP:0x8db0 and its split, mode 2 (the help page)
    std::string w8694, w86d0;          // DGROUP:0x8694 / 0x86d0, mode 3 (the watch's lines)
    std::string w8d38;                 // DGROUP:0x8d38, seg3:0x304c's formatted text
    int bad_index = 0;                 // DGROUP:0x9498's raises (the caller adds them)
};

// seg3:0x31d0(mode, l1, l2) [BYTES seg3:0x31d0-0x332b].
void lines_31d0(ActivityState& st, int mode, std::string l1, std::string l2);

// seg3:0x1d0a(a, s, mode) [BYTES seg3:0x1d0a-0x304b]: a is the viewer (the player's index, or
// 100 when the player watches itself). The demo's give-up writes ship s's words and calls
// ActivityHost::think. Race waves and the duel commentary are logged and leave line 2 empty
// (ruling R3).
void activity_text(world::World& w, ActivityHost& h, ActivityState& st, int a, int s, int mode);

// seg3:0x304c(a, k, mode) [BYTES seg3:0x304c-0x31cf] for defender entry k: its state word
// (DGROUP:0x52fe + 0x24k), owner (+0x06, +0x08) and target (+0x0a, +0x0c).
struct DefenderWords {
    int state = 0, owner_pool = -1, owner_rec = 0, target_pool = -1, target_rec = 0;
};
void defender_text(const world::World& w, ActivityHost& h, ActivityState& st, int a, const DefenderWords& d,
                   int mode);

}  // namespace is::hud
