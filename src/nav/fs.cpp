#include "nav/fs.h"

#include <algorithm>
#include <cstdio>

namespace is::nav {

namespace {

char up(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 0x20) : c; }

bool part_match(const std::string& s, const std::string& m) {
    std::size_t i = 0;
    for (std::size_t j = 0; j < m.size(); ++j) {
        if (m[j] == '*') return true;  // DOS: '*' matches the rest of the part
        if (m[j] == '?') {
            if (i < s.size()) ++i;  // '?' also matches the end of the part
            continue;
        }
        if (i >= s.size() || up(s[i]) != up(m[j])) return false;
        ++i;
    }
    return i == s.size();
}

void split83(const std::string& n, std::string& base, std::string& ext) {
    const auto dot = n.rfind('.');
    if (dot == std::string::npos || n == "." || n == "..") {
        base = n;
        ext.clear();
    } else {
        base = n.substr(0, dot);
        ext = n.substr(dot + 1);
    }
}

}  // namespace

std::string upper(std::string s) {
    for (char& c : s) c = up(c);
    return s;
}

bool dos_attr_match(unsigned entry_attr, unsigned search_attr) {
    const unsigned special = kAttrHidden | kAttrSystem | kAttrDirectory;
    return (entry_attr & special & ~search_attr) == 0 && (entry_attr & kAttrVolume) == 0;
}

bool dos_name_match(const std::string& name, const std::string& mask) {
    if (mask == "*.*" || mask == "*") return true;
    if (name == "." || name == "..") return mask == name;
    std::string nb, ne, mb, me;
    split83(name, nb, ne);
    split83(mask, mb, me);
    if (mask.find('.') == std::string::npos) me = "";
    return part_match(nb, mb) && part_match(ne, me);
}

std::string short_name(const std::string& long_name) {
    if (long_name == "." || long_name == "..") return long_name;
    std::string base, ext;
    split83(long_name, base, ext);
    // spaces and dots dropped, the characters 8.3 names cannot hold replaced by '_'
    const auto clean = [](const std::string& in, std::size_t max) {
        std::string out;
        for (char c : in) {
            if (c == ' ' || c == '.') continue;
            if (c == '+' || c == ',' || c == ';' || c == '=' || c == '[' || c == ']') c = '_';
            if (out.size() < max) out += up(c);
        }
        return out;
    };
    base = clean(base, 8);
    ext = clean(ext, 3);
    return ext.empty() ? base : base + "." + ext;
}

void ShortNames::reserve(const std::string& fitting_name) {
    const std::string n = upper(fitting_name);
    if (std::find(used_.begin(), used_.end(), n) == used_.end()) used_.push_back(n);
}

std::string ShortNames::assign(const std::string& key, const std::string& cut) {
    const auto it = given_.find(key);
    if (it != given_.end()) return it->second;
    std::string base, ext;
    split83(cut, base, ext);
    std::string name = cut;
    for (int n = 1; n <= 999999; ++n) {
        const std::string tail = "~" + std::to_string(n);
        std::string t = base.substr(0, 8 - tail.size()) + tail;
        if (!ext.empty()) t += "." + ext;
        if (std::find(used_.begin(), used_.end(), t) == used_.end()) {
            name = t;
            break;
        }
    }
    used_.push_back(name);
    given_[key] = name;
    return name;
}

bool probe_label_on_listing(DriveType type) {
    switch (type) {
    case DriveType::kFixed:
    case DriveType::kRemovable:
    case DriveType::kCdrom:
    case DriveType::kRamDisk:
        return true;
    default:
        return false;
    }
}

std::vector<ListedDrive> list_drives(Fs& fs, const std::function<void(const std::string&)>& log) {
    std::vector<ListedDrive> out;
    for (const Drive& d : fs.drives()) {
        if (d.letter < 'C' || d.letter > 'Z' || d.type == DriveType::kNoRoot) continue;
        ListedDrive l;
        l.letter = d.letter;
        std::string line = std::string("drive fs ISPACE.DLL seg10:0x2104 findfirst \"") + d.letter + ":\\*.*\" 0x08";
        if (probe_label_on_listing(d.type)) {
            l.probed = true;
            l.has_label = fs.volume_label(d.letter, l.label);
            if (!l.has_label) l.label.clear();
            line += l.has_label ? " -> \"" + l.label + "\"" : std::string(" -> end");
        } else {
            line += " -> not probed (ADR 0012)";
        }
        if (log) log(line);
        out.push_back(std::move(l));
    }
    return out;
}

// ---- MemFs

namespace {
class MemSearch : public Search {
public:
    std::vector<FindEntry> entries;
    std::size_t at = 0;
    bool next(FindEntry& out) override {
        if (at >= entries.size()) return false;
        out = entries[at++];
        return true;
    }
};

std::vector<std::string> components(const std::string& path) {
    // "D:\GAMES\X" -> {"GAMES", "X"} (the drive is taken by the caller)
    std::vector<std::string> v;
    std::string cur;
    for (std::size_t i = 2; i < path.size(); ++i) {
        if (path[i] == '\\') {
            if (!cur.empty()) v.push_back(cur);
            cur.clear();
        } else {
            cur += path[i];
        }
    }
    if (!cur.empty()) v.push_back(cur);
    return v;
}
}  // namespace

void MemFs::add_drive(char letter, DriveType type, const std::string& label) {
    auto& d = drives_[up(letter)];
    d.type = type;
    d.label = label;
}

MemFs::Node* MemFs::lookup(const std::string& dir, bool create) {
    if (dir.size() < 2 || dir[1] != ':') return nullptr;
    auto it = drives_.find(up(dir[0]));
    if (it == drives_.end()) {
        if (!create) return nullptr;
        it = drives_.emplace(up(dir[0]), DriveInfo{}).first;
    }
    Node* n = &it->second.root;
    for (const std::string& c : components(dir)) {
        Node* child = nullptr;
        for (auto& ch : n->children)
            if (upper(ch->name) == upper(c)) child = ch.get();
        if (!child) {
            if (!create) return nullptr;
            n->children.push_back(std::make_unique<Node>());
            child = n->children.back().get();
            child->name = c;
        }
        if (!(child->attr & kAttrDirectory)) return nullptr;
        n = child;
    }
    return n;
}

void MemFs::add_dir(const std::string& path, unsigned attr, bool reparse) {
    Node* n = lookup(path, true);
    if (n) {
        n->attr = attr | kAttrDirectory;
        n->reparse = reparse;
    }
}

void MemFs::add_file(const std::string& path, unsigned attr) {
    const auto slash = path.rfind('\\');
    if (slash == std::string::npos) return;
    Node* dir = lookup(path.substr(0, slash + 1), true);
    if (!dir) return;
    auto f = std::make_unique<Node>();
    f->name = path.substr(slash + 1);
    f->attr = attr & ~kAttrDirectory;
    dir->children.push_back(std::move(f));
}

std::unique_ptr<Search> MemFs::find(const std::string& pattern, unsigned attr) {
    auto s = std::make_unique<MemSearch>();
    const std::string dir = pattern_dir(pattern);
    const std::string mask = pattern.substr(dir.size());
    Node* n = lookup(dir, false);
    if (!n || n->reparse) return s;
    if (components(dir).size() > 0) {
        for (const char* dot : {".", ".."})
            if (dos_attr_match(kAttrDirectory, attr) && dos_name_match(dot, mask))
                s->entries.push_back({dot, kAttrDirectory, false});
    }
    for (const auto& ch : n->children)
        if (dos_attr_match(ch->attr, attr) && dos_name_match(ch->name, mask))
            s->entries.push_back({ch->name, ch->attr, ch->reparse});
    return s;
}

bool MemFs::volume_label(char letter, std::string& label) {
    const auto it = drives_.find(up(letter));
    if (it == drives_.end() || it->second.label.empty()) return false;
    label = it->second.label;
    return true;
}

std::vector<Drive> MemFs::drives() {
    std::vector<Drive> v;
    for (const auto& [letter, d] : drives_)
        if (letter >= 'C' && letter <= 'Z') v.push_back({letter, d.type});
    return v;
}

// ---- paths

bool path_fits(const std::string& dir) { return with_slash(dir).size() + 1 <= kPathBuffer; }

std::string pattern_dir(const std::string& pattern) {
    const auto slash = pattern.rfind('\\');
    if (slash != std::string::npos) return pattern.substr(0, slash + 1);
    if (pattern.size() >= 2 && pattern[1] == ':') return pattern.substr(0, 2);
    return {};
}

std::string with_slash(const std::string& dir) {
    if (!dir.empty() && dir.back() == '\\') return dir;
    return dir + "\\";
}

// ---- calls

bool FsCall::operator==(const FsCall& o) const {
    return routine == o.routine && op == o.op && path == o.path && arg == o.arg && ok == o.ok &&
           name == o.name && value == o.value && refused == o.refused;
}

std::string describe(const FsCall& c) {
    std::string s = c.routine + " ";
    char buf[32];
    switch (c.op) {
    case FsCall::Op::kFindFirst:
        std::snprintf(buf, sizeof buf, "0x%02x", static_cast<unsigned>(c.arg));
        s += "findfirst \"" + c.path + "\" " + buf;
        break;
    case FsCall::Op::kFindNext:
        s += "findnext";
        break;
    case FsCall::Op::kExtractIcon:
        s += "ExtractIcon \"" + c.path + "\" " + std::to_string(c.arg);
        break;
    }
    if (c.refused) return s + " -> refused (ADR 0012)";
    if (c.op == FsCall::Op::kExtractIcon) {
        if (c.arg == 0) return s + (c.value > 1 ? " -> icon" : " -> none");  // a handle above 1
        return s + " -> " + std::to_string(c.value);
    }
    return s + (c.ok ? " -> \"" + c.name + "\"" : std::string(" -> end"));
}

// ---- Walker

Walker::Walker(Fs& fs, Log log) : fs_(fs), log_(std::move(log)) {}

void Walker::push(const FsCall& c) {
    if (sink_) sink_->push_back(c);
    if (log_) log_("nav fs " + describe(c));
}

void Walker::icon_call(const std::string& routine, const std::string& path, int index, int value,
                       bool refused) {
    FsCall c;
    c.routine = routine;
    c.op = FsCall::Op::kExtractIcon;
    c.path = path;
    c.arg = index;
    c.ok = !refused;
    c.value = value;
    c.refused = refused;
    push(c);
}

bool Walker::is_reparse(const std::string& dir) const {
    const std::string d = upper(with_slash(dir));
    for (const std::string& r : reparse_)
        if (d.compare(0, r.size(), r) == 0) return true;
    return false;
}

void Walker::note_reparse(const std::string& path) {
    const std::string d = upper(with_slash(path));
    if (std::find(reparse_.begin(), reparse_.end(), d) == reparse_.end()) reparse_.push_back(d);
}

bool Walker::enterable(const std::string& dir) const { return path_fits(dir) && !is_reparse(dir); }

Walker::Find::Find(Walker& w, std::string routine, std::string pattern, unsigned attr)
    : w_(&w), routine_(std::move(routine)), pattern_(std::move(pattern)), attr_(attr) {}

Walker::Find Walker::find(const std::string& routine, const std::string& pattern, unsigned attr) {
    return Find(*this, routine, pattern, attr);
}

bool Walker::Find::next(FindEntry& out) {
    FsCall c;
    c.routine = routine_;
    c.op = started_ ? FsCall::Op::kFindNext : FsCall::Op::kFindFirst;
    if (!started_) {
        started_ = true;
        c.path = pattern_;
        c.arg = static_cast<int>(attr_);
        const std::string dir = pattern_dir(pattern_);
        if (!w_->enterable(dir)) {
            refused_ = true;
            c.refused = true;
            w_->push(c);
            return false;
        }
        search_ = w_->fs_.find(pattern_, attr_);
    } else if (refused_) {
        return false;
    }
    if (!done_ && delivered_ >= kMaxEntries) {
        c.refused = true;
        w_->push(c);
        done_ = true;
        return false;
    }
    FindEntry e;
    if (done_ || !search_ || !search_->next(e)) {
        done_ = true;
        w_->push(c);
        return false;
    }
    ++delivered_;
    if (e.reparse && (e.attr & kAttrDirectory) && e.name != "." && e.name != "..")
        w_->note_reparse(pattern_dir(pattern_) + e.name);
    c.ok = true;
    c.name = e.name;
    w_->push(c);
    out = e;
    return true;
}

// ---- probes

bool is_subdir(const FindEntry& e) {
    // attribute bit 0x10 and a name not starting with '.' [BYTES seg6:0x3942 seg6:0x394b]
    return (e.attr & kAttrDirectory) && !e.name.empty() && e.name[0] != '.';
}

bool Walker::has_exe(const std::string& dir, const char* routine) {
    // lstrcpy, '\' when missing (DGROUP:0xeb8), "*.exe" (DGROUP:0xeba), findfirst attribute 0
    Find f = find(routine, with_slash(dir) + "*.exe", kFileAttr);
    FindEntry e;
    return f.next(e);
}

bool Walker::has_subdir(const std::string& dir) {
    Find f = find("seg6:0x4748", with_slash(dir) + "*.*", kTreeAttr);
    FindEntry e;
    // A failed find-first goes straight to the find-next (seg6:0x47cf).
    if (f.next(e) && is_subdir(e)) return true;
    while (f.next(e))
        if (is_subdir(e)) return true;
    return false;
}

bool Walker::subdir_has_exe(const std::string& dir) {
    const std::string d = with_slash(dir);
    Find f = find("seg6:0x4650", d + "*.*", kTreeAttr);
    FindEntry e;
    // The find-first's result is not tested: the find buffer is examined either way; after a
    // failed find-first the port's buffer is empty [DESIGN: the original reads a stale buffer].
    // A reparse-point subdirectory's probe is refused by find() [DESIGN ADR 0012 decision 3].
    if (f.next(e) && is_subdir(e) && has_exe(d + e.name)) return true;
    while (f.next(e))
        if (is_subdir(e) && has_exe(d + e.name)) return true;
    return false;
}

bool Walker::passes(const std::string& path, const std::string& root) {
    const bool is_root = root == path;  // lstrcmp(DGROUP:0xd18, path) before the '\' [BYTES seg6:0x45be]
    if (has_exe(path)) return true;
    if (is_root) return false;
    return has_subdir(path) && subdir_has_exe(path);
}

}  // namespace is::nav
