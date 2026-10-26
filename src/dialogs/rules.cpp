#include "dialogs/rules.h"

#include <algorithm>
#include <cctype>

namespace is::dialogs {

namespace {

std::int16_t word16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

std::string upper_ascii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// The name without its last 4 characters (".SAV"), as lstrlen - 4 cuts it.
std::string without_extension(const std::string& file) {
    return file.size() > 4 ? file.substr(0, file.size() - 4) : std::string();
}

std::uint16_t rd16(const std::vector<std::uint8_t>& b, std::size_t o) {
    return static_cast<std::uint16_t>(b[o] | (b[o + 1] << 8));
}
std::uint32_t rd32(const std::vector<std::uint8_t>& b, std::size_t o) {
    return static_cast<std::uint32_t>(rd16(b, o)) | (static_cast<std::uint32_t>(rd16(b, o + 2)) << 16);
}

}  // namespace

int x5_for_edition(int edition) {
    if (edition == 0) return 1;
    if (edition == 3) return 3;
    return 0;
}

int date_number(int tm_year, int tm_yday) { return word16((tm_year - 93) * 365 + tm_yday); }

std::vector<std::string> restore_game_names(const std::vector<std::string>& sav_files,
                                            int* found) {
    std::vector<std::string> out;
    for (const std::string& f : sav_files) {
        const std::string name = without_extension(f);
        if (name == "DEMOGAME") continue;  // lstrcmp, exact
        out.push_back(name);
    }
    if (found) *found = static_cast<int>(sav_files.size());
    return out;
}

std::vector<std::string> save_game_names(const std::vector<std::string>& sav_files, int* added) {
    std::vector<std::string> out;
    for (const std::string& f : sav_files) {
        const std::string name = without_extension(f);
        if (name == "LASTGAME") continue;
        out.push_back(name);
    }
    if (added) *added = static_cast<int>(out.size());
    return out;
}

std::vector<std::string> sorted_like_combo(std::vector<std::string> items) {
    std::stable_sort(items.begin(), items.end(), [](const std::string& a, const std::string& b) {
        return upper_ascii(a) < upper_ascii(b);
    });
    return items;
}

int combo_height(int h, int n) { return h * n + 30; }
int play_combo_height(int h) { return 2 * (h + 15); }

int restore_game_result(bool ok, int index) {
    if (!ok) return 0;
    return index == 0 ? 1 : 2;
}

std::optional<SaveHeader> parse_save_header(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < kSaveHeaderRead) return std::nullopt;
    SaveHeader h;
    std::size_t end = 4;
    while (end < 4 + 20 && bytes[end] != 0) ++end;
    h.pilot.assign(reinterpret_cast<const char*>(bytes.data() + 4), end - 4);
    h.score = static_cast<std::int32_t>(rd32(bytes, 0x1e));
    h.ship = word16(rd16(bytes, 0x22));
    h.wave = word16(rd16(bytes, 0x24));
    h.preview = bytes[0x170];
    return h;
}

std::vector<IniWrite> restore_game_writes(const std::string& combo_text, int ship_index,
                                          const std::string& ship_name,
                                          const std::string& pilot) {
    std::vector<IniWrite> w{{"Restore", combo_text}};
    if (ship_index >= 0) {
        w.push_back({"MainShip", ship_name});
        w.push_back({"ThePlayer", pilot});
    }
    return w;
}

bool limited_body(int body_word) {
    static constexpr int kRefused[] = {178, 310, 320, 400, 440, 450, 520, 550, 560, 590, 600,
                                       604, 605, 710, 720, 730, 750, 890, 900, 930, 950};
    return std::find(std::begin(kRefused), std::end(kRefused), body_word) != std::end(kRefused);
}

int availability_cache(int command2, int command10) { return command2 != 0 ? 0 : command10; }

bool ship_allowed(const Availability& a, int ship) {
    if (a.cache != 0 && a.x5 != 3) return true;
    if (a.x5 == 3 && ship != 1) return false;
    int word = 0;
    if (a.ships && ship >= 0 && static_cast<std::size_t>(ship) < a.ships->records.size()) {
        const std::size_t body = a.ships->records[static_cast<std::size_t>(ship)].body;
        if (a.bodies && body < a.bodies->records.size())
            word = word16(a.bodies->records[body].bitmap_id);
    }
    return !limited_body(word);
}

std::vector<int> new_game_ships(const Availability& a) {
    std::vector<int> out;
    if (!a.ships) return out;
    for (std::size_t i = 0; i < a.ships->records.size(); ++i) {
        const fmt::ShipRecord& r = a.ships->records[i];
        if (r.name.empty() || r.team == 7) continue;
        if (!ship_allowed(a, static_cast<int>(i))) continue;
        out.push_back(static_cast<int>(i));
    }
    return out;
}

int restore_ship_index(int ship_word, const Availability& a) {
    const int count = a.ships ? static_cast<int>(a.ships->records.size()) : 0;
    int ship = ship_word;
    if (ship > count - 1) ship = 0;
    if (!ship_allowed(a, ship)) ship = 1;
    return ship;
}

DefaultShip default_ship(const std::string& main_ship, const Availability& a) {
    DefaultShip d;
    d.name = main_ship;
    if (a.x5 != 0) {
        d.name = kTransformer;
    } else if (a.ships) {
        for (std::size_t i = 0; i < a.ships->records.size(); ++i) {
            const fmt::ShipRecord& r = a.ships->records[i];
            if (r.name != main_ship) continue;
            if (r.team == 7 || !ship_allowed(a, static_cast<int>(i))) {
                d.name = kTransformer;
                d.write_main_ship = true;
            }
            break;
        }
    }
    if (a.ships)
        for (std::size_t i = 0; i < a.ships->records.size(); ++i)
            if (a.ships->records[i].name == d.name) {
                d.record = static_cast<int>(i);
                break;
            }
    return d;
}

bool limited_ship_refused(int x5, const std::string& ship) {
    return x5 != 0 && ship != kTransformer;
}

int zoom_index(int set_size) { return 2 - set_size; }
int size_for_zoom(int index) { return 2 - index; }

std::string label_text(const std::optional<std::string>& name) {
    if (!name || name->empty()) return "Unknown_HD";
    std::string s = *name;
    if (s.size() > 7 && s.size() > 8 && s[8] == '.') s.erase(8, 1);
    return s;
}

std::optional<std::string> label_as_find_first(const std::string& label) {
    if (label.empty()) return std::nullopt;
    if (label.size() <= 8) return label;
    return label.substr(0, 8) + "." + label.substr(8, 3);
}

std::string drive_entry(char letter, const std::optional<std::string>& name) {
    return std::string(1, letter) + ": " + label_text(name);
}

int drive_index(const std::vector<std::string>& entries, const std::string& drive_key) {
    const char want = drive_key.empty() ? '\0' : drive_key[0];
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (!entries[i].empty() && entries[i][0] == want) return static_cast<int>(i);
    return 0;
}

std::string drive_text(int offset, const std::string& colon,
                       const std::optional<std::string>& name) {
    return std::string(1, static_cast<char>('C' + offset)) + colon + label_text(name);
}

std::vector<IniWrite> new_game_writes(bool demo, const std::string& ship,
                                      const std::string& drive_entry_text, int zoom,
                                      const std::string& pilot, int play) {
    std::vector<IniWrite> w;
    w.push_back({"MainShip", ship});
    w.push_back({"Drive", drive_entry_text.substr(0, 1)});
    if (!demo) {
        const std::string size = std::to_string(size_for_zoom(zoom));
        w.push_back({"Size", size});
        w.push_back({"SetSize", size});
        w.push_back({"ThePlayer", pilot});
        w.push_back({"PlayMode", std::to_string(play)});
    }
    return w;
}

bool strat_ask(int play, int strat_ask_key) {
    return play != 0 && (strat_ask_key == 0 || strat_ask_key == 10);
}

bool save_exit_enabled(int x4, int x5) { return x5 != 0 && x4 >= 1 && x4 <= 3; }
int save_exit_argument(int state) { return state == 2 ? -1 : 1; }
bool benefits_on_exit(int edition) { return edition == 0 || edition == 1; }

PlusCountStep plus_count_step(int plus, int edition, int plus_count) {
    PlusCountStep s;
    s.count = plus_count;
    if (plus != 0 || (edition != -1 && edition != 3)) return s;
    s.acts = true;
    s.count = plus_count + 1;
    if (s.count > 12) {
        s.count = 0;
        s.plus_pack = true;
    }
    return s;
}

bool save_dialog_opens(int arg, int save_prompt, const std::string& restore9) {
    if (arg != 1 && arg != -1 && arg != 3) return true;
    if (save_prompt != 0) return true;
    return restore9.empty() || (restore9.size() > 3 && restore9[3] == ' ');
}

std::optional<std::string> save_name(const std::string& combo_text) {
    std::string s = combo_text.substr(0, 8);
    const auto sp = s.find(' ');
    if (sp != std::string::npos) s.resize(sp);
    if (s.empty() || s == "LASTGAME") return std::nullopt;
    return s;
}

bool save_destroys_window(int arg) { return arg == -1 || arg == 1; }

DayText order_day_text(int x4) {
    if (x4 == 0) return {2344, std::nullopt};
    if (x4 > 0) return {2341, x4};
    return {2342, -x4};
}

int order_x4(int invalid, int day) { return invalid == 0 ? day : 0; }
bool order_tshirt(int x5) { return x5 < 0x4c0; }
bool order_phone_hidden(int x4) { return x4 < 3 || x4 > 50; }

std::string format_ints(const std::string& fmt, const std::vector<int>& args) {
    std::string out;
    std::size_t next = 0;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '%' && i + 1 < fmt.size() && fmt[i + 1] == 'd') {
            out += next < args.size() ? std::to_string(args[next]) : std::string();
            ++next;
            ++i;
        } else if (fmt[i] == '%' && i + 1 < fmt.size() && fmt[i + 1] == '%') {
            out += '%';
            ++i;
        } else {
            out += fmt[i];
        }
    }
    return out;
}

std::optional<std::string> order_serial(const std::vector<std::uint8_t>& record) {
    if (record.size() < 10) return std::nullopt;
    const int count = word16(rd16(record, 4));
    const auto serial = static_cast<std::int32_t>(rd32(record, 6));
    if (count > 8 || serial == 0) return std::nullopt;
    return std::to_string(serial);
}

bool needs_important(int edition, int accept) { return edition == 2 && accept != kAccepted; }
bool restore_refused(int edition, int player) { return edition == 0 && player != 0; }

Placement centred(int left, int top, int right, int bottom, int screen_cx, int screen_cy) {
    return {screen_cx / 2 + (left - right) / 2, screen_cy / 2 + (top - bottom) / 2, right - left,
            bottom - top};
}

bool bold_child(const std::string& class_name, std::uint32_t style) {
    if (upper_ascii(class_name) != "BUTTON") return false;
    const std::uint32_t low = style & 0xff;
    return low == 0 || low == 1;  // BS_PUSHBUTTON, BS_DEFPUSHBUTTON
}

std::string resource_text(const std::vector<std::uint8_t>& bytes) {
    std::string s;
    for (std::uint8_t b : bytes) {
        if (b == 0) break;
        s.push_back(static_cast<char>(b));
    }
    return s;
}

std::string team_key(int team) { return "Team" + std::to_string(team + 1); }

}  // namespace is::dialogs
