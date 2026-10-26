// The manual's HTML export (src/help, ADR 0005): the PNG writer, and the export of the real
// help files (skipped without IS_ORIGINAL_DIR): one page per topic, every context id of the
// map resolves, every link and picture names an exported file, the seg2:0x190a contexts
// open the topics the original opens, and the export is stable across runs.
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "assets/game_files.h"
#include "doctest.h"
#include "formats/hlp.h"
#include "help/export.h"
#include "help/png.h"

namespace fs = std::filesystem;
using namespace is;

namespace {

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    return fs::path(v);
}

struct TempDir {
    fs::path path;
    TempDir() {
        const auto n = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("is_help_test_" + std::to_string(n));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::uint32_t be32(const std::vector<std::uint8_t>& b, std::size_t o) {
    return (std::uint32_t{b[o]} << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3];
}

// Every value of `attr`="..." in the HTML.
std::vector<std::string> attr_values(const std::string& html, const std::string& attr) {
    std::vector<std::string> out;
    const std::string key = " " + attr + "=\"";
    for (std::size_t p = html.find(key); p != std::string::npos; p = html.find(key, p + 1)) {
        const std::size_t a = p + key.size();
        out.push_back(html.substr(a, html.find('"', a) - a));
    }
    return out;
}

std::vector<std::uint8_t> help_bytes(const fs::path& dir) {
    const assets::GameFiles gf(dir);
    return gf.has("ENG_HELP.HLP") ? gf.read("ENG_HELP.HLP") : std::vector<std::uint8_t>();
}

void check_export(const std::vector<std::uint8_t>& hlp, std::size_t topics, std::size_t unmapped_ids) {
    const fmt::HlpFile hf(hlp);
    const help::HtmlManual m(hf);
    const auto files = m.render();
    std::set<std::string> names;
    std::size_t pages = 0;
    for (const auto& f : files) {
        CHECK(names.insert(f.name).second);
        if (f.name.size() > 3 && f.name[2] == '-') ++pages;
    }
    CHECK(pages == topics);
    CHECK(names.count(help::kContentsPage));
    CHECK(names.count(help::kKeywordsPage));
    CHECK(names.count(help::kStylesheet));
    for (const auto& [name, pic] : hf.pictures()) CHECK(names.count(name + ".png"));

    // Every context id of the map opens an exported topic: its own, or the contents topic
    // for the ids the file maps to offset -1 (19 and 38-40 in the 2000 file, 19 in 1998).
    REQUIRE(!hf.ctxomap().empty());
    const fmt::HlpTopic* contents = hf.contents_topic();
    REQUIRE(contents != nullptr);
    std::size_t unmapped = 0;
    for (const auto& [id, off] : hf.ctxomap()) {
        INFO("map id " << id);
        const fmt::HlpTopic* t = hf.topic_for_map_id(id);
        if (!t) {
            CHECK(off == -1);
            ++unmapped;
        }
        CHECK(m.page_for_context(static_cast<std::uint32_t>(id)) == m.topic_file((t ? t : contents)->index));
    }
    CHECK(unmapped == unmapped_ids);
    // Every context name resolves to a topic.
    for (const auto& [hash, off] : hf.contexts()) CHECK(hf.topic_at(off) != nullptr);

    // Every hotspot of every topic resolves to an exported page.
    for (const auto& t : hf.topics())
        for (int li : t.links()) {
            INFO("topic " << t.index << " link " << li);
            const std::string target = m.link_target(t, li);
            CHECK(!target.empty());
            CHECK(names.count(target));
        }
    // Every href and src of every HTML page names an exported file.
    for (const auto& f : files) {
        if (f.name.size() < 5 || f.name.compare(f.name.size() - 5, 5, ".html") != 0) continue;
        const std::string html(f.bytes.begin(), f.bytes.end());
        CHECK(html.find("deadlink") == std::string::npos);
        for (const char* attr : {"href", "src"})
            for (const auto& v : attr_values(html, attr)) {
                INFO(f.name << " " << attr << "=" << v);
                CHECK((v == "#" || names.count(v)));
            }
    }
    // Rendering twice gives the same bytes.
    const auto again = m.render();
    REQUIRE(again.size() == files.size());
    for (std::size_t i = 0; i < files.size(); ++i) CHECK(again[i].bytes == files[i].bytes);
}

}  // namespace

TEST_CASE("help: CRC-32 and Adler-32") {
    const std::string s = "123456789";
    CHECK(help::crc32(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()) == 0xCBF43926u);
    const std::string w = "Wikipedia";
    CHECK(help::adler32(reinterpret_cast<const std::uint8_t*>(w.data()), w.size()) == 0x11E60398u);
}

TEST_CASE("help: PNG of an indexed DIB") {
    fmt::Dib d;
    d.width = 3;
    d.height = 2;
    d.bpp = 8;
    d.palette = {0x000000, 0xff0000, 0x00ff00};
    d.bits = {0, 1, 2, 0, 2, 1, 0, 0};  // bottom-up rows of 4 bytes
    const auto png = help::png_from_dib(d);
    REQUIRE(png.size() > 8 + 25);
    CHECK(png[1] == 'P');
    CHECK(be32(png, 8) == 13);  // IHDR length
    CHECK(std::string(png.begin() + 12, png.begin() + 16) == "IHDR");
    CHECK(be32(png, 16) == 3);
    CHECK(be32(png, 20) == 2);
    CHECK(png[25] == 3);  // indexed colour
    CHECK(be32(png, 29) == help::crc32(png.data() + 12, 17));
    // PLTE then IDAT: one stored block holding the top row first.
    std::size_t o = 33;
    CHECK(be32(png, o) == 9);
    o += 12 + 9;
    CHECK(std::string(png.begin() + static_cast<long>(o) + 4, png.begin() + static_cast<long>(o) + 8) == "IDAT");
    const std::size_t z = o + 8;
    CHECK(png[z] == 0x78);
    CHECK(png[z + 2] == 1);  // final stored block
    const std::vector<std::uint8_t> rows(png.begin() + static_cast<long>(z) + 7, png.begin() + static_cast<long>(z) + 15);
    CHECK(rows == std::vector<std::uint8_t>{0, 2, 1, 0, 0, 0, 1, 2});

    d.bits[0] = 7;  // an index past the palette: RGB instead
    const auto rgb = help::png_from_dib(d);
    CHECK(rgb[25] == 2);
}

TEST_CASE("help: HTML export of the 2000 manual") {
    const fs::path orig = env_dir("IS_ORIGINAL_DIR");
    const auto hlp = orig.empty() ? std::vector<std::uint8_t>() : help_bytes(orig);
    if (hlp.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR has no ENG_HELP.HLP");
        return;
    }
    check_export(hlp, 37, 4);

    // The seg2:0x190a contexts (docs/re/shell.md "Help"): 0x1c Credits, 0 and F1/F2 the
    // contents topic, 5 General; an id the map lacks opens the contents topic.
    const fmt::HlpFile hf(hlp);
    const help::HtmlManual m(hf);
    CHECK(m.page_for_context(0x1c) == "30-credits.html");
    CHECK(m.page_for_context(0) == "01-contents.html");
    CHECK(m.page_for_context(5) == "06-general.html");
    CHECK(m.page_for_context(999) == "01-contents.html");

    TempDir tmp;
    const fs::path dir = tmp.path / "help" / "ENG";
    const auto r1 = help::ensure_export(hlp, dir, 0x1c);
    REQUIRE(r1.ok);
    CHECK(r1.regenerated);
    CHECK(r1.files_written == 37 + 3);
    CHECK(r1.page == dir / "30-credits.html");
    CHECK(fs::is_regular_file(dir / help::kContentsPage));
    CHECK(fs::is_regular_file(dir / help::kStampFile));

    // A second run finds the stamp and writes nothing.
    const auto r2 = help::ensure_export(hlp, dir, 5);
    REQUIRE(r2.ok);
    CHECK_FALSE(r2.regenerated);
    CHECK(r2.page == dir / "06-general.html");

    // Without the stamp the export runs again and finds every file unchanged.
    fs::remove(dir / help::kStampFile);
    std::ofstream(dir / "99-stale.html") << "old";
    const auto r3 = help::ensure_export(hlp, dir, 0);
    REQUIRE(r3.ok);
    CHECK(r3.regenerated);
    CHECK(r3.files_written == 0);
    CHECK(r3.files_removed == 1);
    CHECK_FALSE(fs::exists(dir / "99-stale.html"));

    // A changed help file (another hash) is exported again.
    std::ofstream(dir / help::kStampFile, std::ios::binary) << "0000 1\n";
    const auto r4 = help::ensure_export(hlp, dir, 0);
    CHECK(r4.regenerated);

    // A file that is not a help file fails without throwing.
    const auto bad = help::ensure_export(std::vector<std::uint8_t>(32, 0), tmp.path / "bad", 0);
    CHECK_FALSE(bad.ok);
    CHECK(!bad.error.empty());
}

TEST_CASE("help: HTML export of the 1998 manual (pictures)") {
    const fs::path orig = env_dir("IS_ORIGINAL_DIR");
    const fs::path works = orig.empty() ? fs::path() : orig.parent_path() / "theworks";
    const auto hlp = works.empty() || !fs::is_directory(works) ? std::vector<std::uint8_t>() : help_bytes(works);
    if (hlp.empty()) {
        MESSAGE("skipped: no theworks ENG_HELP.HLP beside IS_ORIGINAL_DIR");
        return;
    }
    check_export(hlp, 40, 1);
    const fmt::HlpFile hf(hlp);
    const help::HtmlManual m(hf);
    CHECK(m.page_for_context(0x1c) == "33-credits.html");
    CHECK(m.page_for_context(5) == "06-general.html");
    std::size_t imgs = 0;
    for (const auto& f : m.render())
        if (f.name.size() > 5 && f.name.compare(f.name.size() - 5, 5, ".html") == 0) {
            const std::string html(f.bytes.begin(), f.bytes.end());
            for (std::size_t p = html.find("<img "); p != std::string::npos; p = html.find("<img ", p + 1)) ++imgs;
        }
    CHECK(imgs == 5);
}
