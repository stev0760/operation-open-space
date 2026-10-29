#include "nav/tree.h"

#include <algorithm>

namespace is::nav {

namespace {
char up(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 0x20) : c; }
bool ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (up(a[i]) != up(b[i])) return false;
    return true;
}
bool iprefix(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && ieq(s.substr(0, p.size()), p);
}
int digit(const std::string& t, std::size_t from_end) {
    if (t.size() < from_end) return 0;
    return static_cast<signed char>(t[t.size() - from_end]) - '0';
}
}  // namespace

std::string load(Strings& s, int id, int max) {
    std::string t = s.lang(id);
    const auto nul = t.find('\0');
    if (nul != std::string::npos) t.resize(nul);
    if (max <= 0) return {};
    if (t.size() > static_cast<std::size_t>(max - 1)) t.resize(static_cast<std::size_t>(max - 1));
    return t;
}

// ---- rows

std::string row_text(const std::string& name, int depth, int sub, int state) {
    std::string t = name;
    t += static_cast<char>('0' + depth);  // DGROUP:0xeb2 "0" + depth
    t += static_cast<char>('0' + sub);    // DGROUP:0xeb4 "0" + sub
    t += static_cast<char>('0' + state);  // DGROUP:0xeb6 "0" + state
    return t;
}

RowDigits parse_row(const std::string& text) {
    RowDigits d;
    if (text.size() < 3) {
        d.name = text;
        return d;
    }
    d.name = text.substr(0, text.size() - 3);
    d.depth = digit(text, 3);
    d.sub = digit(text, 2);
    d.state = digit(text, 1);
    return d;
}

// ---- records

int Records::find(const std::string& path) const {
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].path == path) return static_cast<int>(i);
    return -1;
}

int Records::enter(const std::string& path, bool* added) {
    // DGROUP:0x94b0 = 1, then the lookup (lstrcmp of +0x152) [BYTES seg6:0x36b6 seg6:0x36ed]
    const int i = find(path);
    if (i >= 0) {
        if (added) *added = false;
        current = i;  // DGROUP:0x94a2 [BYTES seg6:0x36f3]
        return i;
    }
    if (static_cast<int>(list.size()) >= kMax) {  // [DESIGN] [OPEN]
        if (added) *added = false;
        return -1;
    }
    DirRecord r;
    r.path = path.substr(0, 109);  // +0x152, 110 bytes [BYTES seg6:0x3788]
    list.push_back(r);
    current = static_cast<int>(list.size()) - 1;
    if (added) *added = true;
    return current;
}

bool ends_with(const std::string& s, const std::string& tail) {
    return tail.size() <= s.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

int state_digit(const std::string& path, const Game& g, const Records& r, Strings& s) {
    const std::string p = with_slash(path);  // DGROUP:0xeaa "\" [BYTES seg6:0x3f8e]
    if (g.edition == 0 && g.player) {
        // " - RACE\" and "Lazy Loop - RACE\" and " - DUEL\" [BYTES seg6:0x3ff3 seg6:0x400f]
        const std::string race = load(s, kStrRaceSuffix, 0x28) + "\\";
        const std::string lazy = load(s, kStrRace0, 0x28) + race;
        int st = 0;
        if (ends_with(p, race)) {
            st = kStateLocked;
            if (ends_with(p, lazy)) st = kStateNew;
        }
        if (ends_with(p, load(s, kStrDuelSuffix, 0x28) + "\\")) st = kStateLocked;
        if (st == kStateLocked) return kStateLocked;
    }
    if (g.edition != -1 && g.edition != 2) {
        // "DEMON'S GATE\" [BYTES seg6:0x404e seg6:0x406b]
        if (ends_with(p, load(s, kStrDemonsGate, 0x28) + "\\")) return kStateLocked;
    }
    const int i = r.find(p);  // [BYTES seg6:0x40ff seg6:0x4107]
    if (i >= 0) return r.list[static_cast<std::size_t>(i)].state == 2 ? kStateComplete : kStateVisited;
    return kStateNew;
}

// ---- the list

bool list_less(const std::string& a, const std::string& b) {
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto x = static_cast<unsigned char>(up(a[i])), y = static_cast<unsigned char>(up(b[i]));
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}

Tree::Tree(Walker& walker, Strings& strings, const Records& records, Game game)
    : w_(walker), s_(strings), r_(records), g_(game) {}

void Tree::add_sorted(const std::string& text) {
    const auto it = std::upper_bound(rows_.begin(), rows_.end(), text, list_less);
    const int index = static_cast<int>(it - rows_.begin());
    rows_.insert(it, text);
    edits_.push_back({ListEdit::Op::kInsert, index, text});
}

void Tree::insert(int index, const std::string& text) {
    index = std::clamp(index, 0, static_cast<int>(rows_.size()));
    rows_.insert(rows_.begin() + index, text);
    edits_.push_back({ListEdit::Op::kInsert, index, text});
}

void Tree::erase(int index) {
    if (index < 0 || index >= static_cast<int>(rows_.size())) return;
    rows_.erase(rows_.begin() + index);
    edits_.push_back({ListEdit::Op::kDelete, index, {}});
}

std::vector<ListEdit> Tree::take_edits() {
    std::vector<ListEdit> e;
    e.swap(edits_);
    return e;
}

void Tree::fill(const std::string& root) {
    root_ = root;
    // DGROUP:0x94b2 [BYTES seg6:0x37f7 seg6:0x380d]; DGROUP:0x94b6 = 0 [BYTES seg6:0x3813]
    all_complete_ = g_.edition == -1 || g_.edition == 2;
    count_ = 0;
    rows_.clear();
    edits_.push_back({ListEdit::Op::kReset, 0, {}});  // LB_RESETCONTENT [BYTES seg6:0x3843]

    // Root row: " " + "X:\" + suffix [BYTES seg6:0x386a-0x3909]
    {
        const bool pass = w_.passes(root_, root_);
        const int sub = w_.has_subdir(root_) && w_.subdir_has_exe(root_) ? 1 : 0;
        const int state = pass ? state_digit(root_, g_, r_, s_) : kStateComplete;
        if (state < 2) all_complete_ = false;  // [BYTES seg6:0x38c1]
        add_sorted(" " + row_text(root_, 1, sub, state));
        ++count_;
    }

    // Top-level rows: "X:\*.*" attribute 0x15 [BYTES seg6:0x3931-0x3a04]
    {
        Walker::Find f = w_.find("seg6:0x37ca", root_ + "*.*", kTreeAttr);
        FindEntry e;
        bool got = f.next(e);  // the find-first's result is stored, not tested [BYTES seg6:0x3937]
        do {
            if (got && is_subdir(e)) {
                const std::string p = root_ + e.name;
                if (!w_.enterable(p)) {
                    // Too long or a reparse point: shown, never probed or expanded (M3 s1 fixer)
                    // [DESIGN ADR 0012 decision 3].
                    const int state = state_digit(p, g_, r_, s_);
                    if (state < 2) all_complete_ = false;
                    add_sorted(row_text(e.name, 1, 0, state));
                    ++count_;
                } else if (g_.tree_key == 0 || w_.passes(p, root_)) {
                    int sub = 0;
                    if (w_.has_subdir(p) && (g_.tree_key == 0 || w_.subdir_has_exe(p))) sub = 1;
                    const int state = state_digit(p, g_, r_, s_);
                    if (state < 2) all_complete_ = false;  // [BYTES seg6:0x39ce]
                    add_sorted(row_text(e.name, 1, sub, state));
                    ++count_;
                }
            }
            got = f.next(e);
        } while (got);
    }

    // Built-in rows, depth 1 and sub 0 [BYTES seg6:0x3a2a-0x3bbe]
    {
        const int dg = g_.edition != -1 && g_.edition != 2 ? kStateLocked : kStateNew;
        fill_tail_ = row_text(load(s_, kStrDemonsGate, 0x28), 1, 0, dg);
        add_sorted(fill_tail_);
        ++count_;
        const int races = g_.edition == 3 ? 1 : 3;  // [BYTES seg6:0x3a75]
        for (int i = 0; i < races; ++i) {
            const int st = g_.edition == 0 && i > 0 && g_.player ? kStateLocked : kStateNew;
            fill_tail_ = row_text(load(s_, kStrRace0 + i, 0x28) + load(s_, kStrRaceSuffix, 0x28), 1, 0, st);
            add_sorted(fill_tail_);
            ++count_;
        }
        const int st = g_.edition == 0 && g_.player ? kStateLocked : kStateNew;  // [BYTES seg6:0x3b86]
        fill_tail_ = row_text(load(s_, kStrDuel0, 0x28) + load(s_, kStrDuelSuffix, 0x28), 1, 0, st);
        add_sorted(fill_tail_);
        ++count_;
    }
}

int Tree::sub_digit(int index) const {
    if (index == 0) return digit(root_, 2);  // ':' - '0' [BYTES seg6:0x5260]
    if (index < 0 || index >= static_cast<int>(rows_.size())) return 0;
    return digit(rows_[static_cast<std::size_t>(index)], 2);
}

std::string Tree::name(int index) const {
    if (index == 0) return root_;
    if (index < 0 || index >= static_cast<int>(rows_.size())) return {};
    return parse_row(rows_[static_cast<std::size_t>(index)]).name;
}

std::string Tree::path(int index) const {
    // seg6:0x5318 [BYTES seg6:0x5318]
    if (index <= 0) return root_;
    if (index >= static_cast<int>(rows_.size())) return {};
    std::string p = name(index);
    const int depth = digit(rows_[static_cast<std::size_t>(index)], 3);
    int j = index - 1;
    for (int k = 1; k < depth; ++k) {
        bool found = false;
        while (!found && j >= 0) {
            const std::string& t = rows_[static_cast<std::size_t>(j)];
            if (digit(t, 3) - depth == -k) {
                p = parse_row(t).name + "\\" + p;  // DGROUP:0xee1 "\"
                found = true;
            }
            --j;
        }
    }
    return root_ + p + "\\";  // DGROUP:0xee3 "\"
}

bool Tree::expand(int index, const std::string& dir) {
    if (index < 0 || index >= static_cast<int>(rows_.size())) return false;
    const int before = count_;
    std::string t = rows_[static_cast<std::size_t>(index)];
    if (t.size() < 3) return false;
    t[t.size() - 2] = '2';  // [BYTES seg6:0x4fae]
    erase(index);
    insert(index, t);
    const int depth = digit(t, 3);
    const int at = index + 1;
    // DGROUP:0x6990 + "*.*" (DGROUP:0xedc), attribute 0x15 [BYTES seg6:0x4ffe seg6:0x5020]
    Walker::Find f = w_.find("seg6:0x4f46", dir + "*.*", kTreeAttr);
    FindEntry e;
    bool got = f.next(e);
    do {
        if (got && is_subdir(e)) {
            const std::string child = dir + e.name;
            if (!w_.enterable(child)) {
                // Too long or a reparse point: shown with sub 0, never probed or expanded (M3 s1
                // fixer) [DESIGN ADR 0012 decision 3].
                insert(at, row_text(e.name, depth + 1, 0, state_digit(child, g_, r_, s_)));
                ++count_;
            } else if (w_.passes(child, root_)) {  // [BYTES seg6:0x5065]
                const int sub = w_.has_subdir(child) && w_.subdir_has_exe(child) ? 1 : 0;
                const int state = state_digit(child, g_, r_, s_);
                insert(at, row_text(e.name, depth + 1, sub, state));  // [BYTES seg6:0x50db]
                ++count_;
            }
        }
        got = f.next(e);
    } while (got);
    return before < count_;
}

void Tree::collapse(int index) {
    if (index < 0 || index >= static_cast<int>(rows_.size())) return;
    std::string t = rows_[static_cast<std::size_t>(index)];
    if (t.size() < 3) return;
    t[t.size() - 2] = '1';  // [BYTES seg6:0x5182]
    erase(index);
    insert(index, t);
    const int depth = digit(t, 3);
    const int total = static_cast<int>(rows_.size());  // LB_GETCOUNT
    for (int k = index + 1; k < total; ++k) {
        if (index + 1 >= static_cast<int>(rows_.size())) break;
        if (digit(rows_[static_cast<std::size_t>(index + 1)], 3) <= depth) break;
        erase(index + 1);
    }
}

void Tree::set_state(int index, int state) {
    if (index < 0 || index >= static_cast<int>(rows_.size())) return;
    std::string t = rows_[static_cast<std::size_t>(index)];
    if (t.empty()) return;
    t.back() = static_cast<char>('0' + state);
    erase(index);
    insert(index, t);
}

int Tree::find_exact(int start, const std::string& s) const {
    const int n = static_cast<int>(rows_.size());
    for (int k = 1; k <= n; ++k) {
        const int i = ((start < 0 ? -1 : start) + k) % n;
        if (ieq(rows_[static_cast<std::size_t>(i)], s)) return i;
    }
    return -1;
}

int Tree::find_prefix(int start, const std::string& s) const {
    const int n = static_cast<int>(rows_.size());
    for (int k = 1; k <= n; ++k) {
        const int i = ((start < 0 ? -1 : start) + k) % n;
        if (iprefix(rows_[static_cast<std::size_t>(i)], s)) return i;
    }
    return -1;
}

// ---- the initial selection

InitialSelection initial_selection(Tree& tree, Strings& s, const Records& r, const Game& g,
                                   const InitialInputs& in) {
    InitialSelection out;
    out.demo_named = in.demo_named;
    std::string dir = in.current;
    std::string key = tree.fill_tail();  // bp-0x80 as the fill left it
    const std::string& root = tree.root();
    int slashes = 0;  // bp-0x10

    if (in.wave != 0) {
        // the current record's +0x152, prefixed with the root when byte 1 is not ':' [BYTES
        // seg6:0x3e17]
        dir.clear();
        if (r.current >= 0 && r.current < static_cast<int>(r.list.size()))
            dir = r.list[static_cast<std::size_t>(r.current)].path;
        if (dir.size() < 2 || dir[1] != ':') dir = root + dir;
    } else {
        out.demo_named = false;  // DGROUP:0x94be = 0 [BYTES seg6:0x3c06]
        if (!in.command_line) {
            dir = root;
        } else {
            if (in.demo_type == 520) dir = root;  // [BYTES seg6:0x3c5e]
            else if (in.demo_type == 521 || in.demo_type == 522) out.demo_named = true;
            if (out.demo_named) {
                // INI `Track`, 0x6d bytes [BYTES seg6:0x3c84]
                dir = in.track.substr(0, std::min<std::size_t>(in.track.size(), 0x6c));
                if (in.demo_type == 521) {
                    for (int i = 0; i < 3; ++i)
                        if (load(s, kStrRace0 + i, 0x28) == dir) out.race = i * 4 + 1;  // [BYTES seg6:0x3cf6]
                    dir += load(s, kStrRaceSuffix, 0x28);
                } else {
                    dir = "Dodge City";  // DGROUP:0xe9f [BYTES seg6:0x3d4a]
                    for (int i = 0; i < 1; ++i)
                        if (load(s, kStrDuel0 + i, 0x28) == dir) out.duel = i * 4 + 1;
                    dir += load(s, kStrDuelSuffix, 0x28);
                }
                key = dir;
            }
        }
    }

    // seg6:0x3d93
    int sel = 0;
    if (dir.size() < 4) {
        sel = g.player ? 1 : 0;  // [BYTES seg6:0x3d9c seg6:0x3da1]
    } else {
        if (!out.demo_named) {
            // the first component below the root, and the count of '\' after it [BYTES
            // seg6:0x3e29]
            key.clear();
            bool first_done = false;
            for (std::size_t i = 3; i < dir.size(); ++i) {
                if (dir[i] == '\\') {
                    first_done = true;
                    ++slashes;
                } else if (!first_done) {
                    key += dir[i];
                }
            }
        }
        sel = tree.find_exact(-1, key);  // [BYTES seg6:0x3e40]
        if (sel == -1) sel = tree.find_prefix(-1, key);
        if (sel < 0) {  // [BYTES seg6:0x3e47-0x3e55]
            sel = 0;
            dir += root;
        }
    }

    if (slashes > 1) {
        // each component in turn, the last included [BYTES seg6:0x3e6e-0x3f31]
        const std::string saved = dir;
        int seen = 0;
        std::string comp;
        for (std::size_t i = 3; i < saved.size(); ++i) {
            if (saved[i] != '\\') {
                comp += saved[i];
                continue;
            }
            if (seen != 0) {
                int f = tree.find_exact(sel, comp);
                // nav.md "Initial selection" step 2: the prefix search starts at -1, the top
                // (M3 s3 fixer, C003) [BYTES seg6:0x3ec5 seg6:0x3ee0].
                if (f == -1) f = tree.find_prefix(-1, comp);
                sel = f;
            }
            ++seen;
            // seg6:0x5260 on the selection, seg6:0x4f46 with DGROUP:0x6990 cut after this
            // component [BYTES seg6:0x3f34]
            if (sel >= 0 && tree.sub_digit(sel) == kSubCollapsed) tree.expand(sel, saved.substr(0, i + 1));
            comp.clear();
        }
        dir = saved;
        if (sel < 0) sel = 0;  // [DESIGN: LB_SETCURSEL(-1) leaves no selection in the original]
    }
    out.index = sel;
    out.dir = dir;
    return out;
}

}  // namespace is::nav
