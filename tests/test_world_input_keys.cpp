// The command and watch keys of the switch seg11:0x6b64 (src/world/input.h; work/analysis/m5/s4-
// commands.md section 0 with its verifier's claims; docs/re/pilots.md "Commands"): which routine
// each letter calls under each Ctrl and Alt state, in the order the bytes call them.
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "world/input.h"

using namespace is::world;

namespace {

class Acts : public KeyActions {
public:
    std::vector<std::string> log;
    void ambulance(bool ctrl, bool alt) override { log.push_back(std::string("amb") + (ctrl ? "C" : "") + (alt ? "A" : "")); }
    void unported(std::uint32_t, std::uint16_t key, const char* address) override {
        log.push_back(std::string("unported ") + static_cast<char>(key) + " " + address);
    }
};

struct Rig {
    KeyLinks saved;
    WaveGlobals g;
    KeyState ks;
    Acts a;
    std::vector<std::string> cmds;
    Rig() : saved(key_links()) {
        key_links() = KeyLinks{};
        g.demo = 1;
        g.player = 3;
        key_links().command = [this](Command c, int p) {
            cmds.push_back(std::string(command_address(c)) + "(" + std::to_string(p) + ")");
        };
        key_links().watch = [this](bool c, bool al) {
            cmds.push_back(std::string("watch ") + (c ? "C" : "-") + (al ? "A" : "-"));
        };
    }
    ~Rig() { key_links() = saved; }
    KeyResult press(std::uint16_t key, bool ctrl, bool alt) {
        cmds.clear();
        a.log.clear();
        return key_message(kWmKeyDown, key, g, 0, 0, ctrl, alt, ks, a);
    }
};

}  // namespace

TEST_CASE("keys: the Ctrl commands call their routine with the player") {
    Rig r;
    struct Row { char key; const char* routine; };
    const Row rows[] = {{'A', "seg13:0x8570"}, {'C', "seg13:0x9ce0"}, {'D', "seg13:0x8e98"}, {'E', "seg13:0x9f3e"},
                        {'F', "seg13:0x915c"}, {'G', "seg13:0xa01c"}, {'H', "seg13:0x81c2"}, {'S', "seg13:0x9810"}};
    for (const Row& row : rows) {
        CHECK(r.press(static_cast<std::uint16_t>(row.key), true, false) == KeyResult::kHandled);
        REQUIRE(r.cmds.size() == 1);
        CHECK(r.cmds[0] == std::string(row.routine) + "(3)");
    }
}

TEST_CASE("keys: without Ctrl the letters reach the Strategy routines, F and Ctrl-less H do not") {
    Rig r;
    r.press('A', false, false);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa186(3)"});
    r.press('C', false, false);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa234(3)"});
    r.press('D', false, false);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa4f4(3)"});
    r.press('E', false, false);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa3c8(3)"});
    r.press('G', false, false);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa47c(3)"});
    r.press('S', false, false);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa568(3)"});
    // F has no Strategy form [BYTES seg11:0x6feb-0x6fef].
    CHECK(r.press('F', false, false) == KeyResult::kHandled);
    CHECK(r.cmds.empty());
}

TEST_CASE("keys: Alt forms") {
    Rig r;
    // Alt+A: seg13:0x9a18 and no Strategy call [BYTES seg11:0x7075-0x7092].
    r.press('A', false, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x9a18(3)"});
    // Ctrl+Alt+A: Ctrl's call, then Alt's.
    r.press('A', true, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x8570(3)", "seg13:0x9a18(3)"});
    // Alt+D falls to the Strategy call [BYTES seg11:0x6f20-0x6f37].
    r.press('D', false, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa4f4(3)"});
    // Alt+C and Alt+G the same as without.
    r.press('C', false, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa234(3)"});
    r.press('G', false, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa47c(3)"});
    // Alt+S falls to the Strategy call; Ctrl+Alt+S is the pack drop alone.
    r.press('S', false, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa568(3)"});
    r.press('S', true, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x4ac4(3)"});
    // E: Alt alone is the defenders' exit, no Strategy call; Ctrl+Alt+E runs both in order.
    r.press('E', false, true);
    CHECK(r.cmds == std::vector<std::string>{"seg11:0x70bb(3)"});
    r.press('E', true, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x9f3e(3)", "seg11:0x70bb(3)"});
    // H: Ctrl+Alt+H runs both and no ambulance.
    r.press('H', true, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x81c2(3)", "seg13:0x9ae8(3)"});
    CHECK(r.a.log.empty());
    r.press('H', false, true);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x9ae8(3)"});
    // F with Alt only: nothing.
    r.press('F', false, true);
    CHECK(r.cmds.empty());
}

TEST_CASE("keys: H without a modifier is the ambulance, with one it is not") {
    Rig r;
    r.press('H', false, false);
    CHECK(r.cmds.empty());
    CHECK(r.a.log == std::vector<std::string>{"amb"});
    r.press('H', true, false);
    CHECK(r.a.log.empty());
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x81c2(3)"});
    // Without the command slot the ambulance's own Ctrl and Alt answers stay.
    key_links().command = nullptr;
    r.press('H', true, false);
    CHECK(r.a.log == std::vector<std::string>{"ambC"});
    r.press('H', false, true);
    CHECK(r.a.log == std::vector<std::string>{"ambA"});
}

TEST_CASE("keys: Ctrl+Alt+X is the self destruct and every other X is consumed") {
    // seg11:0x6f3e and 0x6f46 jump to the switch's end seg11:0x72a1 (review L6).
    Rig r;
    CHECK(r.press('X', true, true) == KeyResult::kHandled);
    CHECK(r.cmds == std::vector<std::string>{"seg16:0x8e4(3)"});
    CHECK(r.press('X', true, false) == KeyResult::kHandled);
    CHECK(r.press('X', false, true) == KeyResult::kHandled);
    CHECK(r.press('X', false, false) == KeyResult::kHandled);
    CHECK(r.cmds.empty());
}

TEST_CASE("keys: W takes every modifier state and passes the demo") {
    Rig r;
    r.press('W', false, false);
    CHECK(r.cmds == std::vector<std::string>{"watch --"});
    r.press('W', true, false);
    CHECK(r.cmds == std::vector<std::string>{"watch C-"});
    r.press('W', false, true);
    CHECK(r.cmds == std::vector<std::string>{"watch -A"});
    r.press('W', true, true);
    CHECK(r.cmds == std::vector<std::string>{"watch CA"});
    // The demo (DGROUP:0x95bc = 0) takes W and no command letter [BYTES seg11:0x6e19-0x6e3e].
    r.g.demo = 0;
    r.press('W', true, false);
    CHECK(r.cmds == std::vector<std::string>{"watch C-"});
    r.press('A', true, false);
    CHECK(r.cmds.empty());
    r.press('H', true, false);
    CHECK(r.cmds.empty());
}

TEST_CASE("keys: the menu messages 0x99e and 0x99b force Ctrl and Alt") {
    Rig r;
    r.cmds.clear();
    key_message(kMsgCtrlKey, 'G', r.g, 0, 0, false, false, r.ks, r.a);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0xa01c(3)"});
    r.cmds.clear();
    key_message(kMsgAltKey, 'H', r.g, 0, 0, false, false, r.ks, r.a);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x9ae8(3)"});
}

TEST_CASE("keys: the gate keeps the commands out while the player is locked out or not in play") {
    Rig r;
    CHECK(key_message(kWmKeyDown, 'A', r.g, 9999, 0, true, false, r.ks, r.a) == KeyResult::kGated);
    r.g.now = 0;
    CHECK(key_message(kWmKeyDown, 'A', r.g, 0, 2, true, false, r.ks, r.a) == KeyResult::kGated);
    CHECK(r.cmds.empty());
    // 'H' passes the gate.
    CHECK(key_message(kWmKeyDown, 'H', r.g, 0, 2, true, false, r.ks, r.a) == KeyResult::kHandled);
    CHECK(r.cmds == std::vector<std::string>{"seg13:0x81c2(3)"});
}

TEST_CASE("keys: an empty slot logs the routine's address") {
    KeyLinks saved = key_links();
    key_links() = KeyLinks{};
    WaveGlobals g;
    g.demo = 1;
    KeyState ks;
    Acts a;
    key_message(kWmKeyDown, 'G', g, 0, 0, true, false, ks, a);
    key_message(kWmKeyDown, 'W', g, 0, 0, false, false, ks, a);
    key_message(kWmKeyDown, kVkDelete, g, 0, 0, false, false, ks, a);
    REQUIRE(a.log.size() == 3);
    CHECK(a.log[0] == "unported G seg13:0xa01c");
    CHECK(a.log[1] == "unported W seg13:0x563c");
    CHECK(a.log[2].find("seg11:0x6ee7") != std::string::npos);
    key_links() = saved;
}
