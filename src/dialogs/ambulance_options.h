// owner: M4 stage 3, track VISIT.
// The Ambulance Options dialog (ENG_LANG.DLL DIALOG 194, "Ambulance Options (To Buy)", procedure
// INSPACE seg16:0x4104) and its Sell dialog (DIALOG 378, "Ship Options (To Sell)", procedure
// seg16:0x42a2): work/analysis/m4/s3-visit.md section 3 with s3-visit-verify.md's verdicts, and
// docs/re/core.md "The options dialog".
//
// The portable part: the entry text seg16:0x5762, the price parser seg16:0x0, the suffix test
// seg16:0x5f0e, the owner-draw rules of seg16:0x590c, the totals seg16:0x5820 and every list
// procedure (the fill seg16:0x4bd8, Add>> seg16:0x44fa, Remove seg16:0x4900, the enable rule
// seg16:0x43f2, OK seg16:0x5b24, the Sell list seg16:0x541c and Sell OK seg16:0x5db0), written
// against ShopList, the LB_ messages the procedures send. The dialog drives a real listbox
// through it; the tests drive ModelList. The Win32 part (the dialogs themselves) is guarded by
// _WIN32.
//
// These are INSPACE's dialogs, not ISPACE.DLL's: they take the HUD font DGROUP:0x90fe through
// WM_SETFONT (no seg10:0x602a), and place themselves with seg16:0x51d6's rule.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "world/purchases.h"

namespace is::world {
class World;
}

namespace is::dialogs {

// Template ids and control ids [BYTES ENG_LANG.DLL DIALOG 194] [BYTES ENG_LANG.DLL DIALOG 378].
constexpr std::uint16_t kDlgAmbulance = 194;   // 0xc2 [BYTES seg16:0x1339]
constexpr std::uint16_t kDlgSell = 378;        // 0x17a [BYTES seg16:0x4204]
constexpr int kIdOptions = 101, kIdToDo = 102;  // 0x65, 0x66: LBS_SORT | LBS_OWNERDRAWFIXED |
                                                // LBS_HASSTRINGS | LBS_EXTENDEDSEL | LBS_NOTIFY
constexpr int kIdAdd = 105, kIdRemove = 106;    // 0x69, 0x6a
constexpr int kIdTotal = 109, kIdCredit = 110, kIdRemaining = 113;  // 0x6d, 0x6e, 0x71
constexpr int kIdSell = 195;                    // 0xc3 "&Sell.."
constexpr int kIdOk = 1;                        // IDOK "&OK"

// ---- the text rules -----------------------------------------------------------------------------
// seg16:0x0: skips one leading ' ' or 'a', and a second one after it, then reads "d.d" as
// 100·d0 + 10·d2, otherwise "dd.d" as 1000·d0 + 100·d1 + 10·d3 (the ASCII digits minus '0', no
// validation) [BYTES seg16:0x0-0xa1].
int shop_price(const std::string& t);
// seg16:0x5762's text: "" + (flag not 0 or -1: " ") + (flag 2: " ") + (price > 999: "a") +
// seg6:0x1010(price, 100, 1) + " - " + name; "" (nothing added) for price <= 0 [BYTES
// seg16:0x5762-0x581f DGROUP:0x115b DGROUP:0x115c DGROUP:0x115e DGROUP:0x1160 DGROUP:0x1162].
std::string shop_entry_text(int price, int flag, const std::string& name);
// seg16:0x5f0e(a, b): a ends with b (no guard when b is the longer: the port answers false)
// [BYTES seg16:0x5f0e] (verified claim 100).
bool ends_with(const std::string& a, const std::string& b);
// seg16:0x590c's grey test, for the Options list (CtlID 0x65) only: the sign-extended 16-bit
// price above the dword DGROUP:0x9e57 and a first character other than ' ' [BYTES
// seg16:0x5a0b-0x5a3c].
bool shop_grey(const std::string& t, std::int32_t credit);
// seg16:0x590c's drawn text: when text[1] = ' ' or text[0] = 'a' or text[1] = 'a' the first
// character goes, and a new leading 'a' becomes ' ' [BYTES seg16:0x5a3e-0x5a6c].
std::string shop_display_text(std::string t);

// ---- the listbox the procedures drive ------------------------------------------------------------
class ShopList {
public:
    virtual ~ShopList() = default;
    virtual int count() const = 0;                         // LB_GETCOUNT
    // LB_GETTEXT: false (LB_ERR) and `buf` untouched for an index outside the list.
    virtual bool text(int i, std::string& buf) const = 0;
    virtual std::vector<int> selected() const = 0;         // LB_GETSELCOUNT + LB_GETSELITEMS
    virtual int add(const std::string& t) = 0;             // LB_ADDSTRING (the list sorts)
    virtual void remove(int i) = 0;                        // LB_DELETESTRING (no-op outside)
    virtual void set_sel(int i, bool on) = 0;              // LB_SETSEL
    virtual void reset() = 0;                              // LB_RESETCONTENT
    virtual void redraw(bool on) { (void)on; }             // WM_SETREDRAW
};

// A model of the sorted extended-selection listbox for the host tests and the port's checks:
// LB_ADDSTRING inserts before the first item that compares greater, case-insensitively by byte
// [DESIGN: the control's own sort is CompareString's; the shop's entries differ in their first
// characters, where the two agree].
class ModelList final : public ShopList {
public:
    int count() const override { return static_cast<int>(items.size()); }
    bool text(int i, std::string& buf) const override;
    std::vector<int> selected() const override;
    int add(const std::string& t) override;
    void remove(int i) override;
    void set_sel(int i, bool on) override;
    void reset() override {
        items.clear();
        sel.clear();
    }
    std::vector<std::string> items;
    std::vector<bool> sel;
};

// What the procedures do outside their two lists.
struct ShopEnv {
    ShopEnv(world::World& w_, world::Shop& s_, world::PurchaseHost& h_, const world::WeaponWords& W_)
        : w(w_), s(s_), h(h_), W(W_) {}
    world::World& w;
    world::Shop& s;
    world::PurchaseHost& h;
    const world::WeaponWords& W;     // DGROUP:0x2f54
    bool player_game = true;         // DGROUP:0x95bc
    std::int16_t edition = 0;        // DGROUP:0x920c (0 the Limited edition)
    std::int16_t plus = 0;           // DGROUP:0x11aa
    int served = 0;                  // DGROUP:0x952e
    std::function<std::string(int)> lang;  // ENG_LANG strings
    // MessageBox(DGROUP:0x9532, text, DGROUP:0x9690 "Inner Space", flags) [BYTES seg16:0x46e9].
    std::function<void(const std::string& text, unsigned flags)> message_box;
    // seg16:0x68d6(DGROUP:0x9532, a, b): MessageBox of strings a + b, MB_ICONEXCLAMATION
    // [BYTES seg16:0x4724].
    std::function<void(int a, int b)> refuse;
    // seg10:0x82a2(-100) and SetActiveWindow(DGROUP:0x9532): the New Ship Body chooser over the
    // dialog [BYTES seg16:0x47f3-0x481b].
    std::function<void()> body_chooser;
    // WritePrivateProfileString("Inner Space", "MainShip", name, "INSPACE.INI") [BYTES seg16:0x49b4].
    std::function<void(const std::string& name)> write_main_ship;
    // PostMessage(hDlg, WM_COMMAND, id, 0) [BYTES seg16:0x48f3 seg16:0x5412].
    std::function<void(int id)> post_command;
    // The demo's 1500 ms busy-wait before OK [BYTES seg16:0x5b2f-0x5b62].
    std::function<void(int ms)> busy_wait;
};

// seg16:0x4bd8(hDlg): the Options list for the PLAYER (DGROUP:0x9646), in this order: NEW SHIP
// BODY, BASIC REPAIRS, REFUEL, the wing repair, DOUBLE GUNS, UNIFY FORCES, POWERUP k = 0 ... 5,
// the weapons w = 1 ... 26; then LB_SETSEL(TRUE, 0) [BYTES seg16:0x4bd8-0x51d3].
void shop_fill(ShopEnv& e, ShopList& options);
// seg16:0x44fa(h): Add>>. `options_dialog` is h = DGROUP:0x9532 (false: the Sell dialog
// DGROUP:0x959c) [BYTES seg16:0x44fa-0x48ff].
void shop_add(ShopEnv& e, bool options_dialog, ShopList& options, ShopList& todo);
// seg16:0x4900(h): Remove [BYTES seg16:0x4900-0x4bd6].
void shop_remove(ShopEnv& e, bool options_dialog, ShopList& options, ShopList& todo);
// seg16:0x43f2(h): Add>> enabled / Remove enabled [BYTES seg16:0x43f2-0x44f8].
bool shop_add_enabled(const ShopEnv& e, const ShopList& options);
bool shop_remove_enabled(const ShopList& todo);
// seg16:0x5b24(hDlg): OK, the to-do flags from the To Be Done lines; DGROUP:0x9532 = 0 is the
// caller's [BYTES seg16:0x5b24-0x5dae].
void shop_ok(ShopEnv& e, const ShopList& todo);
// seg16:0x541c's list: the player's weapons not yet sold, at their sell prices [BYTES
// seg16:0x54ef-0x5761].
void sell_fill(ShopEnv& e, ShopList& options);
// seg16:0x5db0(hDlg): Sell OK, with the delete loop of verified claim 73 (CORRECTED): the count
// is read before the deletions and the index steps on after each, so an item that slides into a
// deleted place is not tested [BYTES seg16:0x5db0-0x5f0c].
void sell_ok(ShopEnv& e, const ShopList& sell_todo, ShopList& options);
// seg16:0x5820(h): the Remaining text (edit 0x71 of the Options dialog, 0x6e of the Sell dialog)
// and the Total text (edit 0x6d) [BYTES seg16:0x5820-0x5906].
std::string shop_remaining_text(const ShopEnv& e);
std::string shop_total_text(const ShopEnv& e, bool options_dialog);
// seg16:0x51d6 / seg16:0x541c's Credit edit: seg6:0xf54(credit, 100, 2) + " " + "ICONS".
std::string shop_credit_text(const ShopEnv& e, std::int32_t credit);
// seg16:0x51d6's placement: x = SM_CXSCREEN/2 - w/2, y = max(50, SM_CYSCREEN/2 - h/2 - 150)
// (signed halves) [BYTES seg16:0x51e7-0x5230] (verified claim 55).
struct ShopPlacement {
    int x = 0, y = 0;
};
ShopPlacement shop_placement(int w, int h, int screen_cx, int screen_cy);

}  // namespace is::dialogs

#ifdef _WIN32
#include <windows.h>

#include "dialogs/dialogs.h"

namespace is::dialogs {

// The dialog's program side: the shop's environment and the HUD font.
struct AmbulanceEnv {
    Env env;              // instance, owner DGROUP:0x16, store, strings, sound, todo
    ShopEnv* shop = nullptr;
    HFONT font = nullptr; // DGROUP:0x90fe
};

// DialogBox(DGROUP:0x9212, 0xc2, DGROUP:0x16, seg16:0x4104): the Options dialog, modal; its
// result is ignored by the visit (verified claim 28) [BYTES seg16:0x1339-0x1344].
int run_ambulance_options(AmbulanceEnv& a);
// The same dialog modeless, for the tests; close_ambulance_options() destroys it.
HWND open_ambulance_options(AmbulanceEnv& a);
void close_ambulance_options();

}  // namespace is::dialogs
#endif
