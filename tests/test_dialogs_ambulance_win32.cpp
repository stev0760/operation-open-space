// The Ambulance Options dialog under Win32 (Wine in CTest): ENG_LANG.DLL DIALOG 194 created
// modeless with the ported procedure seg16:0x4104, its list filled by seg16:0x4bd8 for the
// trial's TRANSFORMER of core-0003, the Credit / Remaining / Total texts, Add>> and OK. Skips
// without the original files (IS_ORIGINAL_DIR).
#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "assets/asset_store.h"
#include "dialogs/ambulance_options.h"
#include "doctest.h"
#include "hud/weapons.h"
#include "world/world.h"

using namespace is::dialogs;
using namespace is::world;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

std::string text_of(HWND d, int id) {
    wchar_t buf[128] = {};
    GetDlgItemTextW(d, id, buf, 128);
    return narrow(buf);
}

}  // namespace

TEST_CASE("DIALOG 194: the Options dialog of the trial's first visit") {
    const auto dir = original_dir();
    if (dir.empty()) return;
    is::assets::AssetStore store(dir);
    World w;
    Shop s;
    WeaponWords W;
    const is::hud::WeaponTable t(0);
    for (int k = 0; k < kWeaponTableBlocks; ++k) W.block(k) = t.block(k);
    WorldPurchaseHost h(w);
    w.design_ratings[0].tank = 1320;
    w.design_ratings[0].special = 3;
    ShipRecord& S = w.ship(0);
    S.w26 = 74;
    S.w28 = 61;
    S.w2a = 74;
    S.w2c = 58;
    S.fuel = 1121;
    set_credit(S, 150);
    S.w0a = 1;
    S.be9 = 1;
    S.slots_cb() = 2;
    S.weapon[1] = 8;
    S.weapon_count[1] = 20;
    set_byte_at(S, 0xac, 1);
    set_byte_at(S, 0xad, 1);
    w.roster.resize(1);
    ShopEnv e(w, s, h, W);
    e.edition = 0;
    e.lang = [&store](int id) {
        auto x = store.find_string(static_cast<std::uint16_t>(id));
        return x ? *x : std::string();
    };
    e.message_box = [](const std::string&, unsigned) {};
    e.refuse = [](int, int) {};
    AmbulanceEnv a;
    a.env.instance = GetModuleHandleW(nullptr);
    a.env.store = &store;
    a.env.lang = e.lang;
    a.shop = &e;
    HWND d = open_ambulance_options(a);
    REQUIRE(d != nullptr);
    CHECK(s.dlg == reinterpret_cast<std::uintptr_t>(d));  // DGROUP:0x9532
    HWND list = GetDlgItem(d, kIdOptions);
    CHECK(SendMessageW(list, LB_GETCOUNT, 0, 0) == 21);
    CHECK(text_of(d, kIdCredit) == "1.50 ICONS");
    CHECK(text_of(d, kIdRemaining) == "1.50 ICONS");
    CHECK(text_of(d, kIdTotal) == "0.0 ICONS");
    CHECK(IsWindowEnabled(GetDlgItem(d, kIdAdd)));      // the first row, REFUEL, selected
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(d, kIdRemove)));
    // Add>> of the selected REFUEL [ORACLE core-0003: Total 0.20, Remaining 1.30 for REFUEL alone].
    SendMessageW(d, WM_COMMAND, kIdAdd, 0);
    CHECK(s.cost == 20);
    CHECK(text_of(d, kIdTotal) == "0.20 ICONS");
    CHECK(SendMessageW(GetDlgItem(d, kIdToDo), LB_GETCOUNT, 0, 0) == 1);
    CHECK_FALSE(IsWindowEnabled(GetDlgItem(d, kIdAdd)));
    // OK: the REFUEL flag (the modeless dialog is not ended by EndDialog).
    SendMessageW(d, WM_COMMAND, IDOK, 0);
    CHECK(s.refuel == 1);
    CHECK(s.dlg == 0);
    close_ambulance_options();
}
#endif
