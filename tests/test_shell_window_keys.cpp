// The start-up reads and the exit write-back of the window keys (formats.md "Window state and
// positions", shell.md "Exit"), through is::settings::IniFile in a temporary directory.
#include <filesystem>
#include <fstream>
#include <string>

#include "doctest.h"
#include "settings/settings.h"
#include "shell/window_keys.h"

using namespace is::shell;
namespace fs = std::filesystem;

namespace {

fs::path temp_ini(const char* name, const std::string& text) {
    const fs::path dir = fs::temp_directory_path() / "is_test_shell_window_keys";
    fs::create_directories(dir);
    const fs::path p = dir / name;
    std::ofstream(p, std::ios::binary) << text;
    return p;
}

std::string slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("window keys: defaults, reads and the write-back") {
    const fs::path p = temp_ini("a.ini",
                                "[Inner Space]\r\nFull=0\r\nShowLeft=10\r\nShowRight=300\r\n"
                                "Menu=1\r\nBorder=0\r\nLanguage=ENGLISH\r\nOnTop=0\r\n"
                                "FrameRateSet=54\r\n");
    is::settings::IniFile ini(p);
    WindowKeys k = read_window_keys(ini);
    CHECK(k.full == 0);
    CHECK(k.show == Rect{10, 0, 300, 0});
    CHECK(k.menu == 1);
    CHECK(k.icon_wnd == 1);
    CHECK(k.vscale == 0);
    CHECK(k.border == 1);  // DGROUP:0xa78's initial 1 until seg6:0xb3e reads Border=0
    CHECK(read_border_menu(ini).border == 0);
    CHECK(read_border_menu(ini).menu == 1);
    CHECK(k.language == "ENGL");  // S 5: four characters and the NUL
    CHECK(read_on_top(ini) == 0);
    CHECK(read_frame_rate(ini) == 54);

    // without a move or resize the Show keys stay as they are
    using is::settings::kGameSection;
    k.show = {1, 2, 3, 4};
    write_window_keys(ini, k);
    CHECK(ini.get_int(kGameSection, "ShowTop", -1) == -1);
    CHECK(ini.get_int(kGameSection, "ShowLeft", -1) == 10);
    CHECK(ini.get_int(kGameSection, "Full", -1) == 0);
    CHECK(ini.get_int(kGameSection, "SmWnd", -1) == 0);
    // after one, all four, then the flag is clear
    k.show_dirty = true;
    write_window_keys(ini, k);
    CHECK_FALSE(k.show_dirty);
    CHECK(ini.get_int(kGameSection, "ShowLeft", -1) == 1);
    CHECK(ini.get_int(kGameSection, "ShowTop", -1) == 2);
    CHECK(ini.get_int(kGameSection, "ShowRight", -1) == 3);
    CHECK(ini.get_int(kGameSection, "ShowBottom", -1) == 4);

    // a new file gets the keys in seg6:0x33e's order
    const fs::path q = temp_ini("b.ini", "");
    fs::remove(q);
    is::settings::IniFile fresh(q);
    WindowKeys d = read_window_keys(fresh);
    CHECK(d.full == 1);
    CHECK(d.show == Rect{});
    d.show = {102, 77, 921, 691};
    d.show_dirty = true;
    write_window_keys(fresh, d);
    fresh.save();
    CHECK(slurp(q) ==
          "[Inner Space]\r\nFull=1\r\nMenu=0\r\nIconWnd=1\r\nVScale=0\r\nSmWnd=0\r\n"
          "SScale=0\r\nShowLeft=102\r\nShowRight=921\r\nShowTop=77\r\nShowBottom=691\r\n");
}

TEST_CASE("window keys: Border and Menu defaults of seg6:0xb3e") {
    const fs::path p = temp_ini("c.ini", "[Inner Space]\r\n");
    is::settings::IniFile ini(p);
    const BorderMenu b = read_border_menu(ini);
    CHECK(b.border == 1);
    CHECK(b.menu == 0);
}
