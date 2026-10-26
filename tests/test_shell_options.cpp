// The port's command line and the asset-directory choice of ADR 0002 decision 1.
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "doctest.h"
#include "shell/options.h"

using namespace is::shell;
namespace fs = std::filesystem;

TEST_CASE("options: --game and --ini") {
    auto r = parse_options({"--game", "C:/games/is", "--ini", "x/INSPACE.INI"});
    CHECK(r.error.empty());
    REQUIRE(r.options.game_dir);
    CHECK(*r.options.game_dir == fs::path("C:/games/is"));
    REQUIRE(r.options.ini);
    CHECK(*r.options.ini == fs::path("x/INSPACE.INI"));

    r = parse_options({"--ini", "a.ini"});
    CHECK(r.error.empty());
    CHECK_FALSE(r.options.game_dir);
}

TEST_CASE("options: errors") {
    CHECK(parse_options({}).error.empty());  // --ini is optional (ADR 0004 decision 2)
    CHECK(parse_options({"--game", "d"}).error.empty());
    CHECK_FALSE(parse_options({"--game", "d"}).options.ini);
    CHECK_FALSE(parse_options({"--ini"}).error.empty());                  // no value
    CHECK_FALSE(parse_options({"--ini", "a", "--ini", "b"}).error.empty());
    CHECK_FALSE(parse_options({"--ini", "a", "DEMO"}).error.empty());     // not read yet
    CHECK_FALSE(parse_options({"--INI", "a"}).error.empty());
}

TEST_CASE("options: the asset directory") {
    const fs::path empty = fs::temp_directory_path() / "is_test_shell_options_empty";
    fs::create_directories(empty);
    std::string why;
    CHECK_FALSE(choose_asset_dir(empty, std::nullopt, &why));
    CHECK_FALSE(why.empty());
    CHECK_FALSE(choose_asset_dir(empty, empty / "missing", &why));

    const char* dir = std::getenv("IS_ORIGINAL_DIR");
    if (!dir || !*dir || !fs::is_directory(dir)) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        fs::remove_all(empty);
        return;
    }
    // the executable's directory wins when it holds the modules; else the named one
    CHECK(choose_asset_dir(dir, empty) == fs::path(dir));
    CHECK(choose_asset_dir(empty, fs::path(dir)) == fs::path(dir));
    CHECK(choose_asset_dir(fs::path(), fs::path(dir)) == fs::path(dir));
    fs::remove_all(empty);
}
