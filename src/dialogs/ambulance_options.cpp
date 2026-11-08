// owner: M4 stage 3, track VISIT.
// The Ambulance Options and Sell dialogs (ambulance_options.h). INSPACE.EXE addresses; the
// verdicts are work/analysis/m4/s3-visit-verify.md's, the Ghidra C and listings those of
// work/analysis/m4/s3-visit-packs/.
#include "dialogs/ambulance_options.h"

#include <algorithm>
#include <cctype>

#include "briefing/briefing.h"
#include "briefing/lang_format.h"
#include "world/world.h"

namespace is::dialogs {

using world::ShipRecord;
using world::add16;
using world::idiv16;
using world::mul16;
using world::sub16;

namespace {

// t[i], or NUL past the end: the procedures read their 80-byte stack buffers by index
// [DESIGN ADR 0016 decision 2: the bytes past the NUL of the original's buffer are unknown; NUL
// stands for them].
char at(const std::string& t, int i) {
    if (i >= 0 && static_cast<std::size_t>(i) < t.size()) return t[static_cast<std::size_t>(i)];
    if (i < 0 || static_cast<std::size_t>(i) > t.size()) {  // past the NUL, which t[size] is
        static bool once = false;
        world::guard_todo(once, "a shop text read past its NUL reads NUL", "seg16:0x4104");
    }
    return '\0';
}
std::string L(const ShopEnv& e, int id) { return e.lang ? e.lang(id) : std::string(); }
// seg10:0xa332(buf, id, 1, args...) with the shop's strings.
std::string fmt1(const ShopEnv& e, int id, std::vector<briefing::FArg> args) {
    std::string out;
    briefing::lang_format_into(out, id, 1, args, [&e](int i) { return L(e, i); });
    return out;
}
void add_entry(ShopList& l, int price, int flag, const std::string& name) {
    const std::string t = shop_entry_text(price, flag, name);
    if (!t.empty()) l.add(t);  // seg16:0x5762 adds nothing for price <= 0
}
std::int8_t sb(const ShipRecord& s, int off) { return world::sbyte_at(s, off); }
// The weapon in slot j, the signed byte +0xb8 + j (j may pass slot 9: the bytes after it,
// +0xc2 ..., as the original reads them) [BYTES seg16:0x4f59].
int weapon_at(const ShipRecord& s, int j) { return sb(s, 0xb8 + j); }
std::int16_t row3(ShopEnv& e, const ShipRecord& s) { return e.h.design_word(s.design_index, 0x28); }

// The weapon test of Add>> and Remove [BYTES seg16:0x4553-0x45f6 seg16:0x4a14-0x4ab1]: the first
// w whose name (string 186 + w) the text ends with; then the first j in 2 ... 7 with text[j - 1]
// = '-'; the entry is a new weapon when text[j + 1 ...] does not start with string 1186
// "RELOAD ". -1: no weapon name; 0: a RELOAD entry or no '-'; 1: a new weapon.
int weapon_kind(const ShopEnv& e, const std::string& t) {
    for (int w = 1; w <= world::kShopWeapons; ++w) {
        if (!ends_with(t, L(e, 186 + w))) continue;
        for (int j = 2; j <= 7; ++j) {
            if (at(t, j - 1) != '-') continue;  // acStack_109[j] is t[j - 1]
            const std::string rl = L(e, 1186);
            for (std::size_t m = 0; m < rl.size(); ++m)
                if (at(t, j + 1 + static_cast<int>(m)) != rl[m]) return 1;
            return 0;
        }
        return 0;
    }
    return -1;
}

}  // namespace

// ---- the text rules -----------------------------------------------------------------------------
int shop_price(const std::string& t) {
    int a = 0;  // [BYTES seg16:0x6-0x2f]
    if (at(t, 0) == ' ' || at(t, 0) == 'a') {
        a = 1;
        if (at(t, 1) == ' ' || at(t, 1) == 'a') a = 2;
    }
    if (at(t, a + 1) == '.')  // [BYTES seg16:0x35-0x5d]
        return (at(t, a + 2) - '0') * 10 + (at(t, a) - '0') * 100;
    return (at(t, a + 3) - '0') * 10 + (at(t, a + 1) - '0') * 100 + (at(t, a) - '0') * 1000;  // [BYTES seg16:0x60-0x9b]
}

std::string shop_entry_text(int price, int flag, const std::string& name) {
    if (price <= 0) return {};
    std::string s;  // DGROUP:0x115b ""
    if (flag != -1) {
        if (flag != 0) s += " ";  // DGROUP:0x115c
        if (flag == 2) s += " ";  // DGROUP:0x115e
    }
    if (price > 999) s += "a";  // DGROUP:0x1160
    s += briefing::hundredths_text(static_cast<std::int16_t>(price), 1);  // seg6:0x1010(price, 100, 1)
    s += " - ";  // DGROUP:0x1162
    s += name;
    return s;
}

bool ends_with(const std::string& a, const std::string& b) {
    if (b.size() > a.size()) return false;  // the original reads before a [DESIGN]
    return std::equal(b.rbegin(), b.rend(), a.rbegin());
}

bool shop_grey(const std::string& t, std::int32_t credit) {
    const std::int32_t p = static_cast<std::int16_t>(shop_price(t));  // `(int)uVar3 >> 0xf`
    return p > credit && at(t, 0) != ' ';
}

std::string shop_display_text(std::string t) {
    if (at(t, 1) == ' ' || at(t, 0) == 'a' || at(t, 1) == 'a') {
        t.erase(0, 1);
        if (at(t, 0) == 'a') t[0] = ' ';
    }
    return t;
}

// ---- ModelList ---------------------------------------------------------------------------------
bool ModelList::text(int i, std::string& buf) const {
    if (i < 0 || i >= count()) return false;
    buf = items[static_cast<std::size_t>(i)];
    return true;
}
std::vector<int> ModelList::selected() const {
    std::vector<int> r;
    for (std::size_t i = 0; i < sel.size(); ++i)
        if (sel[i]) r.push_back(static_cast<int>(i));
    return r;
}
int ModelList::add(const std::string& t) {
    const auto up = [](const std::string& s) {
        std::string u = s;
        for (char& ch : u) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return u;
    };
    const std::string k = up(t);
    std::size_t i = 0;
    while (i < items.size() && up(items[i]) <= k) ++i;
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(i), t);
    sel.insert(sel.begin() + static_cast<std::ptrdiff_t>(i), false);
    return static_cast<int>(i);
}
void ModelList::remove(int i) {
    if (i < 0 || i >= count()) return;
    items.erase(items.begin() + i);
    sel.erase(sel.begin() + i);
}
void ModelList::set_sel(int i, bool on) {
    if (i == -1) {
        std::fill(sel.begin(), sel.end(), on);
        return;
    }
    if (i < 0 || i >= count()) return;
    sel[static_cast<std::size_t>(i)] = on;
}

// ---- seg16:0x4bd8 ------------------------------------------------------------------------------
void shop_fill(ShopEnv& e, ShopList& options) {
    world::World& w = e.w;
    ShipRecord& S = w.ship(w.player);  // the PLAYER, DGROUP:0x9646 [BYTES seg16:0x4bd8]
    world::Shop& s = e.s;
    options.redraw(false);  // WM_SETREDRAW(FALSE)
    options.reset();        // LB_RESETCONTENT
    const std::int16_t tank = e.h.design_word(S.design_index, 0x1e);  // DGROUP:0x96f0
    const std::int16_t r3 = row3(e, S);                               // DGROUP:0x96fa
    std::int16_t price = 0;  // local_86: the wing entry reads it stale (see below)
    const bool edition_ok = e.plus != 0 || e.edition != 3;
    if (s.alive == 0 && edition_ok) {
        add_entry(options, 600, 2, L(e, 865));  // NEW SHIP BODY, flag 2 [BYTES seg16:0x4c1e-0x4d11]
        price = 600;
    } else {
        if (e.player_game && edition_ok) add_entry(options, 600, 0, L(e, 865));
        price = 0;
        for (int k = 0; k < 3; ++k) price = add16(price, sub16(100, world::system_word(S, k)));  // no lamp-row test
        if (e.player_game || price > 100) {
            int flag = e.player_game ? 1 : 2;
            if (s.credit < -350) flag = 0;  // `< -0x15e`
            add_entry(options, price, flag, L(e, 2516));  // BASIC REPAIRS
        }
    }
    // REFUEL: seg10:0xa332(buf, 1176, 1, 0) = "REFUEL" at (tank - fuel)/7 (verified claim 58)
    // [BYTES seg16:0x4d17-0x4d7c].
    if (e.player_game || S.fuel < idiv16(tank, 4)) {  // `(t + (t >> 15 & 3)) >> 2`
        price = idiv16(sub16(tank, S.fuel), 7);
        add_entry(options, price, e.player_game ? 1 : 2, fmt1(e, 1176, {0}));
    }
    // The wing repair [BYTES seg16:0x4d82-0x4f19]: price = 100 - system 3 only with a wing lamp
    // row (else the last price stays, the BASIC or REFUEL one), plus 100 - system 3 again for a
    // shield (+0xe9) or a cloak (+0xe7) ship; kind 5, 4 or lamp row 3; -1 lists nothing.
    if (!(s.credit < -349 || s.alive == 0 || !e.player_game)) {
        const bool missing = !(S.be5 == 0 || (sb(S, 0xac) != 0 && sb(S, 0xad) != 0));
        if (r3 != -1) price = sub16(100, world::system_word(S, 3));
        int kind;
        if (S.be9 != 0) {
            kind = 5;
            price = add16(price, sub16(100, world::system_word(S, 3)));
        } else if (S.be7 != 0) {
            kind = 4;
            price = add16(price, sub16(100, world::system_word(S, 3)));
        } else {
            kind = r3;
        }
        if (!((!e.player_game && price < 0x33 && !missing) || kind == -1)) {
            std::string t;  // DGROUP:0x1151 ""
            if (r3 != -1) {
                t += L(e, 1152);                      // "WING"
                if (kind == 5 || kind == 4) t += "\\";  // DGROUP:0x1152
            }
            if (kind == 5) t += L(e, 1155);  // "SHIELD"
            else if (kind == 4) t += L(e, 1156);  // "CLOAK"
            t += L(e, 1153);  // " REPAIR"
            add_entry(options, price, 1, t);
        }
    }
    if (S.w0a != 2) add_entry(options, 250, 0, fmt1(e, 1185, {0}));  // "DOUBLE GUNS" [BYTES seg16:0x4f1f-0x4f53]
    // UNIFY FORCES: slots 1 ... +0xcb inclusive holding weapon 24 or 25, exactly two
    // (verified claim 59) [BYTES seg16:0x4f59-0x4fba].
    int n = 0;
    for (int j = 1; j <= static_cast<std::int8_t>(S.slots_cb()); ++j)
        if (weapon_at(S, j) == 0x19 || weapon_at(S, j) == 0x18) ++n;
    if (n == 2) add_entry(options, 250, 0, L(e, 1970));
    // POWERUP k: roster byte +0x32 + k below 5, and k < 5 or a wing lamp row; (b + 4)·100, less
    // 200 for k >= 2 [BYTES seg16:0x4fbc-0x5050].
    for (int k = 0; k < world::kPowerups; ++k) {
        const std::int8_t b = static_cast<std::int8_t>(e.h.roster_byte(S.roster_index, 0x32 + k));
        if (b < 5 && (k < 5 || r3 != -1)) {
            std::int16_t p = mul16(add16(b, 4), 100);
            if (k > 1) p = add16(p, -200);
            add_entry(options, p, 0, L(e, 1969) + L(e, 1973 + k));
        }
    }
    // The weapons [BYTES seg16:0x5052-0x5169]: Plus or W(w)+0x36 = -1; an owned weapon (slots
    // 0 ... +0xcb - 1) with ammo (W(w)+0x0a != 0) is "RELOAD " + name at (20 - count)·10, one
    // without ammo is skipped; an owned entry is listed only with +0x0a = 2 or a player game.
    for (int wpn = 1; wpn <= world::kShopWeapons; ++wpn) {
        if (!(e.plus != 0 || e.W.word(wpn, 0x36) == -1)) continue;
        bool owned = false, ammo = false;
        int j = 0;
        for (; j < static_cast<std::int8_t>(S.slots_cb()); ++j) {
            if (weapon_at(S, j) == wpn) {
                owned = true;
                ammo = e.W.word(wpn, 0x0a) != 0;
                break;
            }
        }
        std::int16_t p;
        if (owned) {
            p = mul16(sub16(static_cast<std::int8_t>(S.weapon_count[static_cast<std::size_t>(j < 10 ? j : 0)]), 20), -10);
            if (!ammo) continue;
        } else {
            p = e.W.word(wpn, 0x34);
        }
        std::string t;  // DGROUP:0x1154 ""
        if (owned) t += L(e, 1186);  // "RELOAD "
        t += L(e, 186 + wpn);
        if (!owned || S.w0a == 2 || e.player_game) add_entry(options, p, 0, t);
    }
    options.redraw(true);
    (void)options.count();     // LB_GETCOUNT, the result unused (verified claim 57)
    options.set_sel(0, true);  // LB_SETSEL(TRUE, 0)
}

// ---- seg16:0x44fa ------------------------------------------------------------------------------
void shop_add(ShopEnv& e, bool options_dialog, ShopList& options, ShopList& todo) {
    world::Shop& s = e.s;
    std::vector<int> sel = options.selected();
    const int n = e.player_game ? static_cast<int>(sel.size()) : 0;  // the demo moves nothing (verified claim 60)
    todo.redraw(false);
    options.redraw(false);
    std::string t;
    for (int i = 0; i < n; ++i) {
        options.text(sel[static_cast<std::size_t>(i)], t);
        const std::int16_t price = static_cast<std::int16_t>(shop_price(t));
        if (s.alive != 0 && ends_with(t, "BODY")) {  // DGROUP:0x113c [BYTES seg16:0x47f3-0x481b]
            if (e.body_chooser) e.body_chooser();     // before the refusals (verified claim 66)
        }
        const int kind = weapon_kind(e, t);
        const bool newweapon = kind == 1;
        if (newweapon) s.weapons = add16(s.weapons, options_dialog ? 1 : -1);  // [BYTES seg16:0x45c9-0x45dd]
        // The POWERUP test [BYTES seg16:0x4589-0x4698]: the first character of t equal to
        // "POWERUP "'s 'P' within 12; the verify loop runs only while its index < 4 and compares
        // string byte [i] (i = this selection's index) with t[i + m]. Ported literally: it also
        // catches "6.0 - PROTECTIVE PELLETS" and "5.5 - SPUD MISSILES" [BYTES seg16:0x4589].
        const std::string pw = L(e, 1969);
        bool found = false;
        int m = 0;
        for (; m < 12; ++m) {
            if (at(t, m) == at(pw, 0)) {
                found = true;
                break;
            }
            if (at(t, m) == '\0') break;
        }
        if (found)
            for (; m < 4; ++m)
                if (at(t, i + m) != at(pw, i)) found = false;
        bool ok = true;
        if (options_dialog && s.weapons > 10 && newweapon) {  // [BYTES seg16:0x46a2-0x46ee]
            if (e.player_game && e.message_box) e.message_box(L(e, 2023) + L(e, 2024), 0x30);
            s.weapons = sub16(s.weapons, 1);
            ok = false;
        }
        if (options_dialog && e.edition == 0 && (newweapon || found) && e.player_game) {
            if (e.refuse) e.refuse(2320, 2321);  // the Limited edition [BYTES seg16:0x4700-0x472a]
            ok = false;  // DGROUP:0x958e is not taken back (verified claim 65)
        }
        if (!options_dialog) ok = true;  // [BYTES seg16:0x4730-0x4738]
        if (!ok || (s.credit < price && at(t, 0) != ' ')) {
            options.set_sel(sel[static_cast<std::size_t>(i)], false);  // stays in the list
        } else {
            if (options_dialog) {  // [BYTES seg16:0x4758-0x477a] (verified claim 61)
                s.credit -= price;
                s.cost = add16(s.cost, price);
            } else {
                s.credit += price;
                s.sold_total = add16(s.sold_total, price);
            }
            todo.add(t);
        }
    }
    sel = options.selected();  // the deletion pass, from the last [BYTES seg16:0x4833-0x489e]
    for (int i = 0; i < static_cast<int>(sel.size()); ++i)
        options.remove(sel[sel.size() - 1 - static_cast<std::size_t>(i)]);
    todo.redraw(true);
    options.redraw(true);
    if (!e.player_game && e.post_command) e.post_command(kIdOk);
}

// ---- seg16:0x4900 ------------------------------------------------------------------------------
void shop_remove(ShopEnv& e, bool options_dialog, ShopList& options, ShopList& todo) {
    world::Shop& s = e.s;
    std::vector<int> sel = todo.selected();
    todo.redraw(false);
    options.redraw(false);
    std::string t;
    for (int idx : sel) {
        todo.text(idx, t);
        if (at(t, 0) == ' ' && at(t, 1) == ' ') {  // a flag-2 entry stays (verified claim 67)
            todo.set_sel(idx, false);
            continue;
        }
        options.add(t);
        if (ends_with(t, "BODY") && e.write_main_ship)  // DGROUP:0x1141 (verified claim 68)
            e.write_main_ship(e.h.design_name(e.w.ship(e.served).design_index));
        const std::int16_t price = static_cast<std::int16_t>(shop_price(t));
        if (options_dialog) {
            s.cost = sub16(s.cost, price);
            s.credit += price;
        } else {
            s.sold_total = sub16(s.sold_total, price);
            s.credit -= price;
        }
        if (weapon_kind(e, t) == 1) s.weapons = add16(s.weapons, options_dialog ? -1 : 1);
    }
    sel = todo.selected();
    for (int i = 0; i < static_cast<int>(sel.size()); ++i)
        todo.remove(sel[sel.size() - 1 - static_cast<std::size_t>(i)]);
    todo.redraw(true);
    options.redraw(true);
}

// ---- seg16:0x43f2 ------------------------------------------------------------------------------
bool shop_add_enabled(const ShopEnv& e, const ShopList& options) {
    std::string t;
    bool on = false;
    for (int i : options.selected()) {
        options.text(i, t);
        const std::int32_t p = static_cast<std::int16_t>(shop_price(t));
        if (p <= e.s.credit || at(t, 0) == ' ') on = true;
    }
    return on;
}
bool shop_remove_enabled(const ShopList& todo) { return !todo.selected().empty(); }

// ---- seg16:0x5b24 ------------------------------------------------------------------------------
void shop_ok(ShopEnv& e, const ShopList& todo) {
    world::Shop& s = e.s;
    if (!e.player_game && e.busy_wait) e.busy_wait(1500);  // [BYTES seg16:0x5b2f-0x5b62]
    s.wing = s.refuel = s.basic = s.unify = s.double_guns = s.new_body = 0;
    s.bought.fill(0);
    s.powerup.fill(0);
    const std::string refuel = fmt1(e, 1176, {0}), dguns = fmt1(e, 1185, {0});
    std::string t;
    for (int i = 0; i < todo.count(); ++i) {
        todo.text(i, t);
        if (ends_with(t, L(e, 2516))) s.basic = 1;
        else if (ends_with(t, refuel)) s.refuel = 1;
        else if (ends_with(t, L(e, 1153))) s.wing = 1;
        else if (ends_with(t, dguns)) s.double_guns = 1;
        else if (ends_with(t, L(e, 1970))) s.unify = 1;
        else if (ends_with(t, L(e, 2308))) s.new_body = 1;
        else {
            for (int k = 0; k < world::kPowerups; ++k) {  // one purchase per line (verified claim 70)
                if (ends_with(t, L(e, 1969) + L(e, 1973 + k))) {
                    s.powerup[static_cast<std::size_t>(k)] = 1;
                    break;
                }
            }
            for (int w = 1; w <= world::kShopWeapons; ++w) {
                if (ends_with(t, L(e, 186 + w))) {
                    s.bought[static_cast<std::size_t>(w)] = 1;
                    break;
                }
            }
        }
    }
}

// ---- seg16:0x541c's list -----------------------------------------------------------------------
void sell_fill(ShopEnv& e, ShopList& options) {
    ShipRecord& S = e.w.ship(e.w.player);
    options.redraw(false);
    options.reset();
    for (int wpn = 1; wpn <= world::kShopWeapons; ++wpn) {
        if (e.s.sold[static_cast<std::size_t>(wpn)] != 0) continue;
        for (int j = 0; j < 10; ++j) {  // all ten slots, not +0xcb (verified claim 72)
            if (weapon_at(S, j) != wpn) continue;
            const bool ammo = e.W.word(wpn, 0x0a) != 0;
            std::int16_t p = e.W.word(wpn, 0x34);
            if (p != 0) {
                if (ammo) p = idiv16(mul16(static_cast<std::int8_t>(S.weapon_count[static_cast<std::size_t>(j)]), p), 20);
                if (p < 250) p = 250;  // [BYTES seg16:0x5730]
                add_entry(options, p, 1, L(e, 186 + wpn));
            }
            break;
        }
    }
    options.redraw(true);
    options.set_sel(0, true);
}

// ---- seg16:0x5db0 ------------------------------------------------------------------------------
void sell_ok(ShopEnv& e, const ShopList& sell_todo, ShopList& options) {
    std::string t, buf;
    for (int i = 0; i < sell_todo.count(); ++i) {
        sell_todo.text(i, t);
        int w = 1;
        for (; w <= world::kShopWeapons; ++w)
            if (ends_with(t, L(e, 186 + w))) break;
        if (w > world::kShopWeapons) continue;
        e.s.sold[static_cast<std::size_t>(w)] = 1;
        const std::string name = L(e, 186 + w);
        const int n = options.count();  // read once, before the deletions
        for (int k = 0; k < n; ++k) {
            options.text(k, buf);  // LB_ERR leaves the last text in the buffer
            if (ends_with(buf, name)) options.remove(k);
        }
    }
}

// ---- seg16:0x5820 ------------------------------------------------------------------------------
std::string shop_remaining_text(const ShopEnv& e) {
    return briefing::hundredths_text(e.s.credit, 2) + " " + L(e, 12515);  // DGROUP:0x1166
}
std::string shop_total_text(const ShopEnv& e, bool options_dialog) {
    const std::int16_t v = options_dialog ? e.s.cost : e.s.sold_total;
    return briefing::hundredths_text(v, 2) + " " + L(e, 12515);  // DGROUP:0x1168
}
std::string shop_credit_text(const ShopEnv& e, std::int32_t credit) {
    return briefing::hundredths_text(credit, 2) + " " + L(e, 12515);  // DGROUP:0x1155 / 0x1157
}
ShopPlacement shop_placement(int w, int h, int cx, int cy) {
    int y = idiv16(static_cast<std::int16_t>(cy), 2) - idiv16(static_cast<std::int16_t>(h), 2) - 0x96;
    if (y < 0x32) y = 0x32;
    return {idiv16(static_cast<std::int16_t>(cx), 2) - idiv16(static_cast<std::int16_t>(w), 2), y};
}

}  // namespace is::dialogs

#ifdef _WIN32
#include <memory>

#include "gfx/win16_edit.h"

namespace is::dialogs {

namespace {

// ShopList on a listbox control (LB_ messages; cp1252 through wide/narrow, ADR 0003).
class ListControl final : public ShopList {
public:
    explicit ListControl(HWND h) : h_(h) {}
    int count() const override { return static_cast<int>(SendMessageW(h_, LB_GETCOUNT, 0, 0)); }
    bool text(int i, std::string& buf) const override {
        const LRESULT n = SendMessageW(h_, LB_GETTEXTLEN, static_cast<WPARAM>(i), 0);
        if (n == LB_ERR) return false;
        std::wstring w(static_cast<std::size_t>(n) + 1, L'\0');
        if (SendMessageW(h_, LB_GETTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(w.data())) == LB_ERR)
            return false;
        w.resize(static_cast<std::size_t>(n));
        buf = narrow(w);
        return true;
    }
    std::vector<int> selected() const override {
        // LB_GETSELCOUNT, then LB_GETSELITEMS into the procedures' 100-word buffer
        // [BYTES seg16:0x4515-0x452f].
        LRESULT n = SendMessageW(h_, LB_GETSELCOUNT, 0, 0);
        if (n <= 0) return {};
        if (n > 100) n = 100;
        std::vector<int> v(static_cast<std::size_t>(n));
        const LRESULT got = SendMessageW(h_, LB_GETSELITEMS, static_cast<WPARAM>(n), reinterpret_cast<LPARAM>(v.data()));
        if (got < 0) return {};
        v.resize(static_cast<std::size_t>(got));
        return v;
    }
    int add(const std::string& t) override {
        const std::wstring w = wide(t);
        return static_cast<int>(SendMessageW(h_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str())));
    }
    void remove(int i) override { SendMessageW(h_, LB_DELETESTRING, static_cast<WPARAM>(i), 0); }
    void set_sel(int i, bool on) override { SendMessageW(h_, LB_SETSEL, on ? TRUE : FALSE, static_cast<LPARAM>(i)); }
    void reset() override { SendMessageW(h_, LB_RESETCONTENT, 0, 0); }
    void redraw(bool on) override { SendMessageW(h_, WM_SETREDRAW, on ? TRUE : FALSE, 0); }

private:
    HWND h_;
};

// The controls WM_SETFONT(DGROUP:0x90fe, TRUE) reaches; not the buttons [BYTES seg16:0x5240-0x52f0].
constexpr int kFontIds[] = {0x65, 0x66, 0x67, 0x68, 0x6b, 0x6c, 0x6d, 0x6e, 0x70, 0x71};

// The two procedures' common part: placement, fonts, the owner-draw and the lists.
class ShopDialog : public Dialog {
public:
    ShopDialog(AmbulanceEnv& a, std::uint16_t id, bool options) : Dialog(a.env, id), a_(a), options_(options) {}

protected:
    ShopEnv& e() { return *a_.shop; }
    ListControl list(int id) { return ListControl(GetDlgItem(hwnd_, id)); }
    void place_and_font() {
        const int w = start_rect_.right - start_rect_.left, h = start_rect_.bottom - start_rect_.top;
        const ShopPlacement p = shop_placement(w, h, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        MoveWindow(hwnd_, p.x, p.y, w, h, TRUE);
        if (a_.font)
            for (int id : kFontIds) SendDlgItemMessageW(hwnd_, id, WM_SETFONT, reinterpret_cast<WPARAM>(a_.font), TRUE);
    }
    void totals() {  // seg16:0x5820(h)
        set_text(options_ ? kIdRemaining : kIdCredit, shop_remaining_text(e()));
        set_text(kIdTotal, shop_total_text(e(), options_));
    }
    void enable() {  // seg16:0x43f2(h)
        ListControl o = list(kIdOptions), t = list(kIdToDo);
        EnableWindow(GetDlgItem(hwnd_, kIdAdd), shop_add_enabled(e(), o) ? TRUE : FALSE);
        EnableWindow(GetDlgItem(hwnd_, kIdRemove), shop_remove_enabled(t) ? TRUE : FALSE);
    }
    void add_pressed() {
        ListControl o = list(kIdOptions), t = list(kIdToDo);
        shop_add(e(), options_, o, t);
        totals();
        enable();
    }
    void remove_pressed() {
        ListControl o = list(kIdOptions), t = list(kIdToDo);
        shop_remove(e(), options_, o, t);
        totals();
        enable();
    }
    // The list notifications: seg16:0x43f2, and a double click (LBN_DBLCLK, 2) posts WM_COMMAND
    // 0x69 (list 0x65) or 0x6a (list 0x66) (verified claim 53) [BYTES seg16:0x4240-0x426d].
    void list_command(int id, int code) {
        enable();
        if (code == LBN_DBLCLK) PostMessageW(hwnd_, WM_COMMAND, id == kIdOptions ? kIdAdd : kIdRemove, 0);
    }

    // seg16:0x590c: the owner-draw of one entry [BYTES seg16:0x590c-0x5a89].
    void draw_entry(const DRAWITEMSTRUCT& d) {
        const bool selected = (d.itemState & ODS_SELECTED) != 0;
        HBRUSH br = CreateSolidBrush(GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
        SetTextColor(d.hDC, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        SetBkMode(d.hDC, TRANSPARENT);
        std::string t;
        ListControl(d.hwndItem).text(static_cast<int>(d.itemID), t);
        RECT r = d.rcItem;
        FillRect(d.hDC, &r, br);
        HGDIOBJ old = a_.font ? SelectObject(d.hDC, a_.font) : nullptr;
        if (d.CtlID == kIdOptions && shop_grey(t, e().s.credit)) SetTextColor(d.hDC, GetSysColor(COLOR_GRAYTEXT));
        const std::wstring w = wide(shop_display_text(t));
        TextOutW(d.hDC, r.left + 2, r.top, w.c_str(), static_cast<int>(w.size()));
        if (old) SelectObject(d.hDC, old);
        DeleteObject(br);
    }
    // seg16:0x4104's WM_DRAWITEM: itemID -1 or ODA_FOCUS: seg16:0x5a8a; ODA_DRAWENTIRE:
    // seg16:0x5ada; ODA_SELECT: seg16:0x590c [BYTES seg16:0x4111-0x4170 seg16:0x5a8a seg16:0x5ada].
    void draw_item(const DRAWITEMSTRUCT& d) {
        const auto entire = [&] {  // seg16:0x5ada
            draw_entry(d);
            if (d.itemState & ODS_FOCUS) DrawFocusRect(d.hDC, &d.rcItem);
        };
        if (d.itemID == static_cast<UINT>(-1) || d.itemAction == ODA_FOCUS) {
            if (d.itemID == static_cast<UINT>(-1) || (d.itemState & ODS_FOCUS)) DrawFocusRect(d.hDC, &d.rcItem);
            else entire();
        } else if (d.itemAction == ODA_DRAWENTIRE) {
            entire();
        } else if (d.itemAction == ODA_SELECT) {
            draw_entry(d);
        }
    }
    bool message(UINT msg, WPARAM, LPARAM l, INT_PTR& result) override {
        if (msg == WM_DRAWITEM) {
            draw_item(*reinterpret_cast<const DRAWITEMSTRUCT*>(l));
            result = TRUE;  // [BYTES seg16:0x4279]
            return true;
        }
        if (msg == WM_MEASUREITEM) {
            // itemHeight = tmHeight + tmExternalLeading of DGROUP:0x90fe on GetDC(DGROUP:0x16)
            // [BYTES seg16:0x4172-0x41b8]; returns 0.
            auto* m = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            HDC dc = GetDC(env_.owner);
            HGDIOBJ old = a_.font ? SelectObject(dc, a_.font) : nullptr;
            TEXTMETRICW tm{};
            if (GetTextMetricsW(dc, &tm)) m->itemHeight = static_cast<UINT>(tm.tmHeight + tm.tmExternalLeading);
            if (old) SelectObject(dc, old);
            ReleaseDC(env_.owner, dc);
            result = 0;
            return true;
        }
        return false;
    }
    INT_PTR command_result(int id) const override { return id == IDOK ? 1 : 0; }

    AmbulanceEnv& a_;
    bool options_;
};

// DIALOG 378 with procedure seg16:0x42a2 (INSPACE export 12) [BYTES seg16:0x42a2-0x43f0].
class SellDialog final : public ShopDialog {
public:
    SellDialog(AmbulanceEnv& a, HWND options_dialog) : ShopDialog(a, kDlgSell, false), parent_(options_dialog) {}

protected:
    void init() override {  // seg16:0x541c
        e().s.sell_dlg = reinterpret_cast<std::uintptr_t>(hwnd_);  // DGROUP:0x959c
        place_and_font();
        e().s.sold_total = 0;  // DGROUP:0x9e5d
        totals();              // its Remaining goes into 0x6e, overwritten next
        set_text(kIdCredit, shop_credit_text(e(), world::credit_of(e().w.ship(e().served))));
        ListControl o = list(kIdOptions);
        sell_fill(e(), o);
        enable();
        SetFocus(GetDlgItem(hwnd_, kIdOptions));
    }
    void command(int id, int code) override {
        switch (id) {
        case IDOK: {  // seg16:0x5db0
            ListControl t = list(kIdToDo);
            ListControl po(GetDlgItem(parent_, kIdOptions));
            sell_ok(e(), t, po);
            InvalidateRect(GetDlgItem(parent_, kIdOptions), nullptr, TRUE);
            UpdateWindow(GetDlgItem(parent_, kIdOptions));
            e().s.sell_dlg = 0;
            end(1);
            break;
        }
        case kIdOptions:
        case kIdToDo: list_command(id, code); break;
        case kIdAdd: add_pressed(); break;
        case kIdRemove: remove_pressed(); break;
        default: break;
        }
    }

private:
    HWND parent_;
};

// DIALOG 194 with procedure seg16:0x4104 (INSPACE export 7) [BYTES seg16:0x4104-0x429e].
class OptionsDialog final : public ShopDialog {
public:
    explicit OptionsDialog(AmbulanceEnv& a) : ShopDialog(a, kDlgAmbulance, true) {}

protected:
    void init() override {  // seg16:0x51d6
        ShopEnv& x = e();
        x.s.dlg = reinterpret_cast<std::uintptr_t>(hwnd_);  // DGROUP:0x9532 [BYTES seg16:0x51e7]
        place_and_font();
        x.s.credit = world::credit_of(x.w.ship(x.served));  // DGROUP:0x9e57
        x.s.cost = 0;                                        // DGROUP:0x9e55
        totals();
        set_text(kIdCredit, shop_credit_text(x, x.s.credit));
        ListControl o = list(kIdOptions);
        shop_fill(x, o);
        enable();
        SetFocus(GetDlgItem(hwnd_, kIdOptions));
        if (!x.player_game || x.s.alive == 0) PostMessageW(hwnd_, WM_COMMAND, kIdAdd, 0);  // (verified claim 56)
        x.post_command = [h = hwnd_](int id) { PostMessageW(h, WM_COMMAND, static_cast<WPARAM>(id), 0); };
    }
    void command(int id, int code) override {
        switch (id) {
        case IDOK: {  // seg16:0x5b24
            ListControl t = list(kIdToDo);
            shop_ok(e(), t);
            e().s.dlg = 0;  // DGROUP:0x9532 = 0 before EndDialog (verified claim 71)
            end(1);
            break;
        }
        case kIdOptions:
        case kIdToDo: list_command(id, code); break;
        case kIdAdd: add_pressed(); break;
        case kIdRemove: remove_pressed(); break;
        case kIdSell: {  // [BYTES seg16:0x41f6-0x4232] (verified claim 54)
            sound(0x13);
            AmbulanceEnv inner = a_;
            inner.env.owner = hwnd_;  // owner DGROUP:0x9532
            SellDialog sd(inner, hwnd_);
            sd.run_modal();
            totals();
            enable();
            UpdateWindow(GetDlgItem(hwnd_, kIdOptions));
            break;
        }
        default: break;
        }
    }
};

}  // namespace

namespace {
void default_calls(ShopEnv& x);
std::unique_ptr<Dialog> g_modeless;
}  // namespace

HWND open_ambulance_options(AmbulanceEnv& a) {
    if (!a.shop) return nullptr;
    default_calls(*a.shop);
    close_ambulance_options();
    auto d = std::make_unique<OptionsDialog>(a);
    HWND h = d->create_modeless();
    g_modeless = std::move(d);
    return h;
}
void close_ambulance_options() {
    if (g_modeless && g_modeless->hwnd()) DestroyWindow(g_modeless->hwnd());
    g_modeless.reset();
}

namespace {
void default_calls(ShopEnv& x) {
    // The procedures' outside calls that need the dialog's window (DGROUP:0x9532).
    const auto owner = [&x] { return reinterpret_cast<HWND>(x.s.dlg); };
    if (!x.message_box)
        x.message_box = [owner](const std::string& t, unsigned flags) {
            MessageBoxW(owner(), wide(t).c_str(), L"Inner Space", flags);  // DGROUP:0x9690
        };
    if (!x.refuse)
        x.refuse = [owner, &x](int s1, int s2) {  // seg16:0x68d6
            std::string t = x.lang ? x.lang(s1) : std::string();
            if (s2 != -1 && x.lang) t += x.lang(s2);
            MessageBoxW(owner(), wide(t).c_str(), L"Inner Space", MB_ICONEXCLAMATION);
        };
    if (!x.busy_wait)
        x.busy_wait = [](int ms) {
            const DWORD t0 = GetTickCount();
            while (GetTickCount() - t0 < static_cast<DWORD>(ms)) {
            }
        };
}
}  // namespace

int run_ambulance_options(AmbulanceEnv& a) {
    if (!a.shop) return -1;
    default_calls(*a.shop);
    OptionsDialog d(a);
    const int r = d.run_modal();
    a.shop->s.dlg = 0;
    return r;
}

}  // namespace is::dialogs
#endif
