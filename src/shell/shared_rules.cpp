#include "shell/shared_rules.h"

namespace is::shell {

int day_part(int hour) {
    if (hour >= 3 && hour <= 11) return 0;
    if (hour >= 12 && hour <= 17) return 1;
    if (hour >= 18) return 2;
    return 3;
}

std::string drop_label_dot(std::string s) {
    // lstrlen > 7 and byte 8 is '.': copy down from index 8 [BYTES ISPACE.DLL seg10:0x2197
    // seg10:0x219f seg10:0x21a9 seg10:0x20b8-0x20d4]. A length of 8 has the NUL at index 8.
    if (s.size() > 8 && s[8] == '.') s.erase(8, 1);
    return s;
}

std::string label_text(const std::optional<std::string>& name) {
    if (!name || name->empty()) return "Unknown_HD";
    return drop_label_dot(*name);
}

std::optional<std::string> label_as_find_first(const std::string& label) {
    if (label.empty()) return std::nullopt;
    if (label.size() <= 8) return label;
    return label.substr(0, 8) + "." + label.substr(8, 3);
}

}  // namespace is::shell
