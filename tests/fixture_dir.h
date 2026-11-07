// The fixture drive of oracle/fixtures/make_fixtures.py (tree "basic"), for golden tests.
// IS_FIXTURE_DIR names it; otherwise it is looked for at work/fixtures/basic next to
// IS_ORIGINAL_DIR (work/extracted/game). Empty when neither exists: the tests skip.
#pragma once

#include <cstdlib>
#include <filesystem>

inline std::filesystem::path fixture_dir() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* v = std::getenv("IS_FIXTURE_DIR"); v && *v && fs::is_directory(v, ec))
        return fs::path(v);
    if (const char* v = std::getenv("IS_ORIGINAL_DIR"); v && *v && fs::is_directory(v, ec)) {
        fs::path base(v);
        if (!base.has_filename()) base = base.parent_path();
        const fs::path f = base.parent_path().parent_path() / "fixtures" / "basic";
        if (fs::is_directory(f / "GAMES", ec)) return f;
    }
    return {};
}
