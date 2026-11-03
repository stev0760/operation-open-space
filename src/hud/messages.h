// The message dispatcher seg3:0x6ec (docs/re/core.md "Messages (seg3:0x6ec)", "Lamp messages")
// for the codes the damage, death and repair paths raise, and the formatter parts it uses: the
// message mode of the left IS_INFO display (world.md "Message display (id 0xd2, mode 3)") shows
// its sender DGROUP:0x8740 and its two lines DGROUP:0x8768 / DGROUP:0x87b8. Portable.
//
// Ported from the bytes for M4 stage 1 (track SCREENS): the head seg3:0x6ec-0x83e, the switch
// seg3:0x1346-0x1351, the cases of codes 10-13, 82-84, 91, 94 and 112 (and, M4 stage 2 track
// DISPLAY, the capture's 0x48, 0x55, 0x56 and 0x6f; M4 stage 3 track AMBFLIGHT, the ambulance's
// replies 7, 8 and 132 and the low-fuel warning 9), the tail seg3:0x14d6,
// seg10:0xa332's %S %s %d %? %R %< %> and its mode-2 split, and seg3:0x3498. Every other case of
// the 192 is [OPEN]: the dispatcher shows the sender and switches the display, as the original's
// head does, and leaves the two lines as they were; MessageHost::todo names the code.
#pragma once

#include <cstdint>
#include <string>

namespace is::hud {

// seg3:0x3498(src, line1, line2): src cut at 78 characters; from its middle m = len / 2 it looks
// for a space at m + i, then at m - i, for i = 0 ... m - 1; line 1 ends after that space and line
// 2 holds the rest. With no space line 1 is all of it and line 2 is empty [BYTES
// seg3:0x34a9-0x352d].
void split_lines(const std::string& src, std::string& line1, std::string& line2);

// seg3:0x1542's cut: a name string ends in "^" and a code letter ('M', 'F', 'N'), which the
// routine reads and cuts before it copies the name (347 "Ambulance^N" reads "Ambulance") [BYTES
// seg3:0x1600 seg3:0x163b seg3:0x164c]. The port cuts the two characters when the caret is there
// [DESIGN].
std::string cut_name_code(std::string name);

// seg3:0x14d6: a-z to A-Z in both lines when W > 635 [BYTES seg3:0x14d6-0x1538].
void upper_lines(int W, std::string& line1, std::string& line2);

// The DGROUP words the dispatcher keeps.
struct MessageState {
    std::string sender;            // DGROUP:0x8740
    std::string line1;             // DGROUP:0x8768
    std::string line2;             // DGROUP:0x87b8
    std::string text;              // DGROUP:0x8db0, the formatted text before the split
    std::uint32_t ram_due = 0;     // dword DGROUP:0x1cb4: code 13's sound, at most every 4 s
    int last_rec = 0;              // DGROUP:0x1cb2
    int last_code = 0;             // DGROUP:0x1cb0
};

// What seg3:0x6ec reads and calls outside itself.
class MessageHost {
public:
    virtual ~MessageHost() = default;
    virtual int player() const { return 0; }                // DGROUP:0x9646
    virtual std::uint32_t now() const { return 0; }         // DGROUP:0x1e50
    virtual int width() const { return 1024; }              // DGROUP:0x90ea
    virtual bool race() const { return false; }             // DGROUP:0x94e
    virtual std::string pilot() const { return {}; }        // DGROUP:0x380c, ThePlayer
    virtual std::string lang(int) { return {}; }            // LoadString(DGROUP:0x9212, id)
    virtual int rand() { return 0; }                        // seg5:0xdc
    // Ship words +0x32 (DGROUP:0x70c2) and +0x06 (DGROUP:0x7096) of ship `rec`, for code 0xb7's
    // test [BYTES seg3:0x718-0x729].
    virtual int ship_goal_pool(int /*rec*/) const { return -1; }
    virtual int ship_word_06(int /*rec*/) const { return -1; }
    // DGROUP:0x6e70 = rec, DGROUP:0x6e72 = pool: direction mark 1's object [BYTES seg3:0x731
    // seg3:0x737].
    virtual void set_sender(int /*pool*/, int /*rec*/) {}
    // DGROUP:0xa76.
    virtual int info_mode() const { return 2; }
    // seg7:0x119c(mode, 0): DGROUP:0xa76 = mode and a WM_PAINT to DGROUP:0xa7c while it exists
    // [BYTES seg7:0x11a6-0x11bf].
    virtual void set_info_mode(int /*mode*/) {}
    // DGROUP:0xa7c != 0 [BYTES seg3:0x771].
    virtual bool info_exists() const { return false; }
    // seg11:0x3e6e: unless DGROUP:0xa76 = 4, a WHITENESS PatBlt of the display's client and a
    // WM_PAINT [BYTES seg11:0x3e7b-0x3ecb].
    virtual void info_flash() {}
    // seg3:0x1542(0, pool, rec, DGROUP:0x8740, 0): the sender's name (core.md "Names").
    virtual std::string name(int /*pool*/, int /*rec*/) { return {}; }
    // DGROUP:0x870c = t and DGROUP:0x9608 = 0 [BYTES seg3:0x7b9 seg3:0x7bd seg3:0x825 seg3:0x82c].
    virtual void set_end(std::uint32_t /*t*/) {}
    // seg10:0x744c(id).
    virtual void sound(int /*id*/) {}
    // The capture's words (M4 s2 DISPLAY): DGROUP:0x9604, the captured icon's value in
    // hundredths, and DGROUP:0x9602, the defenders that passed over [BYTES seg3:0xe21-0xe35].
    virtual int capture_value() const { return 0; }
    virtual int capture_defenders() const { return 0; }
    // seg13:0x218a(design word DGROUP:0x970a of ship rec): the team name (M5) [OPEN].
    virtual std::string team_name(int /*ship*/) { return {}; }
    // A case of the switch the port does not carry yet [OPEN].
    virtual void todo(int /*code*/) {}
};

// seg3:0x6ec(recipient, pool, rec, code), cdecl.
void dispatch_message(MessageState& s, MessageHost& h, int recipient, int pool, int rec, int code);

// The codes this port carries (the others reach MessageHost::todo).
bool message_code_ported(int code);

}  // namespace is::hud
