#include "nav/files.h"

#include <cstdint>

#include "intro/texts.h"

namespace is::nav {

int MemIcons::count(const std::string& path) {
    const auto it = counts_.find(upper(path));
    return it == counts_.end() ? 0 : it->second;
}

int builtin_icon_id(char key) {
    switch (key) {  // [BYTES seg6:0x43d0-0x43e8]
    case 'S': return 105;
    case 'R': return 106;
    case 'D': return 107;
    default: return 0;
    }
}

namespace {

std::int16_t add16(int a, int b) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(a + b)); }

// seg6:0x4368's state across one seg6:0x5524 call: DGROUP:0x94c0 (the preview is set) and
// DGROUP:0x94c2 (something reached the bitmap), both cleared by seg6:0x5524 [BYTES seg6:0x5536].
struct IconState {
    bool set = false;      // DGROUP:0x94c0
    bool reached = false;  // DGROUP:0x94c2
};

// seg6:0x4368(dir, name) [BYTES seg6:0x4368].
void icon_routine(Walker& w, IconCounter* icons, const std::string& dir, const std::string& name,
                  FilePane& pane, IconState& st) {
    int id = 0;
    if (dir.size() == 1) id = builtin_icon_id(dir[0]);  // [BYTES seg6:0x43d8]
    // '\' (DGROUP:0xecc) when the last byte is not one; an empty dir gives "\" [BYTES seg6:0x441c]
    const std::string path = with_slash(dir) + name;
    int n = 0;
    if (!name.empty() && icons) {
        n = icons->count(path);
    }
    // the root and built-in queries add 0 under the port [DESIGN ADR 0012 decision 5]
    w.icon_call("seg6:0x4368", path, -1, n);
    if (!name.empty() && n < 1) n = 1;  // [BYTES seg6:0x4441]
    pane.icons = add16(pane.icons, n);  // [BYTES seg6:0x444a]
    if (st.set) return;
    bool handle = false;  // a handle above 1
    if (name.empty()) {
        handle = id != 0;  // LoadIcon(hISPACE, id) [BYTES seg6:0x448c seg6:0x4498]
    } else {
        handle = icons && icons->has_icon(path);
        w.icon_call("seg6:0x4368", path, 0, handle ? 2 : 0);  // [BYTES seg6:0x4469]
    }
    if (!(handle || name.empty() || !st.reached)) return;
    if (handle || name.empty()) st.set = true;
    if (!st.reached || handle) pane.preview = Preview{};  // PatBlt BLACKNESS [BYTES seg6:0x44cb]
    if (handle) {
        if (name.empty()) {
            pane.preview.kind = Preview::Kind::kBuiltin;
            pane.preview.icon_id = id;
        } else {
            pane.preview.kind = Preview::Kind::kIcon;  // DrawIcon [BYTES seg6:0x44ec]
            pane.preview.path = path;
        }
    } else if (pane.icons != 0 && !st.reached) {
        pane.preview.kind = Preview::Kind::kPlaceholder;  // seg9:0x352 [BYTES seg6:0x451f-0x455c]
        pane.preview.name = name;
    }
    st.reached = true;
}

// seg6:0x420a(dir, pattern) [BYTES seg6:0x420a seg6:0x4263 seg6:0x426a].
int walk_pattern(Walker& w, IconCounter& icons, const std::string& dir, const char* pattern,
                 FilePane& pane, IconState& st) {
    Walker::Find f = w.find("seg6:0x420a", with_slash(dir) + pattern, kFileAttr);
    FindEntry e;
    if (!f.next(e)) return 0;
    int n = 0;
    do {
        ++n;
        icon_routine(w, &icons, dir, e.name, pane, st);
        pane.items.push_back(e.name + " 0");  // "" + name + " " + "0" + digit 0
        pane.paths.push_back(with_slash(dir) + e.name);
    } while (f.next(e));
    return n;
}

}  // namespace

FilePane walk_files(Walker& w, IconCounter& icons, const std::string& dir) {
    FilePane pane;
    IconState st;
    pane.exes = walk_pattern(w, icons, dir, "*.exe", pane, st);  // DGROUP:0xef1 [BYTES seg6:0x5877]
    pane.dlls = walk_pattern(w, icons, dir, "*.dll", pane, st);  // DGROUP:0xef7 [BYTES seg6:0x5889]
    icon_routine(w, nullptr, "", "", pane, st);                  // [BYTES seg6:0x589b]
    return pane;
}

FilePane builtin_pane(Walker& w, char key) {
    FilePane pane;
    IconState st;
    icon_routine(w, nullptr, std::string(1, key), "", pane, st);
    return pane;
}

// ---- status

std::string format(Strings& s, const std::string& fmt, const std::vector<int>& args) {
    std::string out;
    std::size_t a = 0;
    const auto arg = [&]() { return a < args.size() ? args[a++] : 0; };
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '%' && i + 1 < fmt.size() && (fmt[i + 1] == '#' || fmt[i + 1] == '?')) {
            if (fmt[i + 1] == '#') {
                const int n = static_cast<std::int16_t>(arg());
                const int id = arg();
                out += std::to_string(n) + " " + s.lang(n == 1 ? id : id + 10000);
            } else {
                const int flag = arg();
                const int id0 = arg(), id1 = arg();
                out += s.lang(flag ? id1 : id0);
            }
            ++i;
            continue;
        }
        out += fmt[i];
    }
    return out;
}

std::string status_display(const std::string& raw) { return intro::case_rule(raw, 3); }

RowKind classify(const std::string& dir, Strings& s) {
    // DGROUP:0x6990 without its last byte; strings at most 39 bytes [BYTES seg6:0x55a4 seg6:0x55bd]
    const std::string p = dir.empty() ? dir : dir.substr(0, dir.size() - 1);
    if (ends_with(p, load(s, kStrDemonsGate, 0x28))) return RowKind::kDemonsGate;
    if (ends_with(p, load(s, kStrRaceSuffix, 0x28))) return RowKind::kRace;
    if (ends_with(p, load(s, kStrDuelSuffix, 0x28))) return RowKind::kDuel;
    return RowKind::kDirectory;
}

Selected select(Walker& w, IconCounter& icons, Strings& s, Records& records, const Game& g,
                const std::string& dir, int sub_digit) {
    Selected out;
    out.kind = classify(dir, s);
    out.record = records.find(dir);  // [BYTES seg6:0x5536-0x557b]
    if (out.kind != RowKind::kDirectory) {
        // seg6:0x3f4c on the path with byte 1 set to ' ' [BYTES seg6:0x5617 seg6:0x569d]
        std::string q = dir;
        if (q.size() > 1) q[1] = ' ';
        const bool locked = state_digit(q, g, records, s) == kStateLocked;
        int first = kStrGateway, second = kStrGatewayEnter;
        const char* sep = " ";  // DGROUP:0xee6
        char key = 'S';
        if (out.kind == RowKind::kDemonsGate) {
            out.box = 0;
        } else if (out.kind == RowKind::kRace) {
            out.box = 1;
            first = kStrRaceTrack;
            second = kStrRaceEnter;
            sep = ". ";  // DGROUP:0x9ee
            key = 'R';
        } else {
            out.box = 2;
            first = kStrDuelArena;
            second = kStrDuelEnter;
            sep = ". ";
            key = 'D';
        }
        // A locked built-in keeps its first string and separator; only the second becomes 2323
        // (nav.md "Status line"; M3 s3 fixer, C001) [ORACLE newgame-0001].
        out.status = load(s, first, 0x28) + sep +
                     (locked ? load(s, kStrPayFull, 0x3c) : load(s, second, 0x28));
        out.pane = builtin_pane(w, key);
        out.enter = EnterButton::kEnabled;  // [BYTES seg6:0x569d]
        return out;
    }

    out.box = 0;  // seg6:0x2dba(0) [BYTES seg6:0x586a]
    out.pane = walk_files(w, icons, dir);
    if (out.pane.exes == 0) {
        // 938 + 942 + ". " + 939 [BYTES seg6:0x58a1-0x5948]
        out.status = load(s, kStrNoApps, 0x3c) + load(s, kStrInThisDir, 0x3c) + ". " +
                     load(s, kStrSelectAnother, 0x3c);
        out.enter = sub_digit == 1 || sub_digit == 2 ? EnterButton::kUnchanged : EnterButton::kDisabled;
        return out;
    }
    const int files = out.pane.exes + out.pane.dlls;  // [BYTES seg6:0x5974]
    std::string buf;
    bool complete = false;
    if (out.record >= 0) {
        DirRecord& r = records.list[static_cast<std::size_t>(out.record)];
        // +0x1d0 = 2, or the signed sum of +0x14e and +0x150 exceeds DGROUP:0x94a6 [BYTES
        // seg6:0x598c seg6:0x5994 seg6:0x599e]
        if (r.state == 2 || static_cast<std::int16_t>(out.pane.icons) < add16(r.destroyed, r.captured)) {
            complete = true;
            buf = load(s, kStrComplete, 400) + ". ";
            if (r.state != 2) {
                r.state = 2;  // [BYTES seg6:0x59b6]
                out.completed = true;
            }
            out.enter = EnterButton::kDisabled;  // [BYTES seg6:0x5a6d]
        }
    }
    if (!complete) {
        out.enter = EnterButton::kEnabled;  // [BYTES seg6:0x5a74]
        // seg10:0xa332(3028, mode 1, DGROUP:0x94a6 = 1, 2056, 2055, DGROUP:0x94a6, 2515, files,
        // 941) [BYTES seg6:0x5a82 seg6:0x5a87]
        buf = format(s, s.lang(kStrIconsInFiles),
                     {out.pane.icons == 1 ? 1 : 0, kStrThereAre, kStrThereIs, out.pane.icons, kStrIcon,
                      files, kStrFile});
    }
    if (out.record >= 0) {
        const DirRecord& r = records.list[static_cast<std::size_t>(out.record)];
        // seg10:0xa332(3029, mode 0, +0x150, 2506, +0x14e, 2506) [BYTES seg6:0x5ad1-0x5b20]
        buf += format(s, s.lang(kStrCapturedDestroyed), {r.captured, kStrIconWas, r.destroyed, kStrIconWas});
    } else {
        buf += load(s, kStrNotBeen, 0x3c) + ". ";  // [BYTES seg6:0x5ab4]
    }
    out.status = buf;
    return out;
}

}  // namespace is::nav
