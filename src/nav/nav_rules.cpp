#include "nav/nav_rules.h"

namespace is::nav {

std::vector<std::pair<std::string, int>> close_writes(int left, int top, int right, int bottom) {
    return {{"NavLeft", left}, {"NavRight", right - left}, {"NavTop", top}, {"NavBottom", bottom - top}};
}

Box enter_box(int cw, int) { return {cw - 105, 5, 90, 26}; }
Box help_box(int cw, int) { return {cw - 105, 40, 90, 26}; }
Box dir_list_box(int, int ch) { return {0, 0, 160, ch - 26}; }

std::string drive_root(int drive_offset) {
    return std::string(1, static_cast<char>('c' + drive_offset)) + ":\\";
}

EnterAction enter_action(bool armed, int wave, int restored) {
    if (!armed) return EnterAction::kRefused;  // DGROUP:0xd20 = 0 [BYTES seg6:0x302e]
    if (wave == 0 || restored != 0) return EnterAction::kRequestWave;
    return EnterAction::kArmDebriefing;
}

}  // namespace is::nav
