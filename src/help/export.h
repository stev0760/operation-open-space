// The manual as HTML (ADR 0005 decisions 2-4). The original hands <Language>_HELP.HLP to
// WinHelp (seg2:0x190a, docs/re/shell.md "Help"), which Windows 10 and 11 no longer ship;
// the port writes every topic of the file as an HTML page under
// <INI directory>\help\<Language>\ and opens the requested one in the user's browser.
//
// The export, per help file:
// - one page per topic, "NN-slug.html" (NN the 1-based topic number, the slug from its
//   title, as `isre hlp` names its Markdown files), with WinHelp's button bar (Contents,
//   Index, Back, and the browse buttons << >> that the file's BrowseButtons() macro adds);
// - "index.html", every topic in file order (the export's contents page);
// - "keywords.html", the keyword index (WinHelp's Search list);
// - "manual.css", the file's fonts and colours as CSS classes;
// - each |bmN picture as "bmN.png";
// - "manual.sha256", the help file's SHA-256 and the exporter version, written last.
// Hotspots become links (jumps solid, popups dotted, as WinHelp underlines them), pictures
// sit inline or float left/right as the 0x86-0x88 commands place them, and the
// non-scrolling region stays at the top of the page.
//
// Portable: the Win32 side (the INI directory, ShellExecuteW) is src/help/hooks.cpp.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "formats/hlp.h"

namespace is::help {

// Raised when the HTML output changes, so that an existing export is written again.
inline constexpr int kExportVersion = 1;

inline constexpr const char* kContentsPage = "index.html";
inline constexpr const char* kKeywordsPage = "keywords.html";
inline constexpr const char* kStylesheet = "manual.css";
inline constexpr const char* kStampFile = "manual.sha256";

struct ExportFile {
    std::string name;  // file name in the export directory (ASCII)
    std::vector<std::uint8_t> bytes;
};

class HtmlManual {
public:
    explicit HtmlManual(const fmt::HlpFile& hf);

    // The pages, the stylesheet and the pictures, in a fixed order; the stamp is not among
    // them. The bytes depend on nothing but the help file.
    std::vector<ExportFile> render() const;

    // "NN-slug.html" of a topic (1-based index).
    const std::string& topic_file(int index) const;
    // The page WinHelp(hwnd, file, cmd, ctx) shows for seg2:0x190a's ctx: 0 is HELP_INDEX,
    // the contents topic; any other value is HELP_CONTEXT, the topic the file's |CTXOMAP
    // maps it to. An id the map lacks opens the contents topic (ADR 0005 decision 3); a
    // file without topics opens index.html.
    std::string page_for_context(std::uint32_t ctx) const;

    // The file name a link of `topic` points to, or empty for a macro, an external jump or
    // an unresolved context.
    std::string link_target(const fmt::HlpTopic& topic, int link) const;

private:
    std::string topic_page(const fmt::HlpTopic& t) const;
    std::string contents_page() const;
    std::string keywords_page() const;
    std::string stylesheet() const;
    std::string page_head(const std::string& title_utf8) const;
    std::string button_bar(const fmt::HlpTopic* t) const;
    std::string paragraph_html(const fmt::HlpTopic& t, const fmt::HlpParagraph& p) const;
    std::string runs_html(const fmt::HlpTopic& t, const fmt::HlpParagraph& p) const;
    std::string text_html(const fmt::HlpRun& r) const;
    std::string display_title(const fmt::HlpTopic& t) const;  // UTF-8

    const fmt::HlpFile& hf_;
    std::vector<std::string> files_;  // by topic index - 1
};

// The SHA-256 stamp line of an export: "<hex digest> <kExportVersion>\n".
std::string export_stamp(const std::vector<std::uint8_t>& hlp);

struct ExportResult {
    bool ok = false;
    bool regenerated = false;      // the stamp differed, so the files were checked and written
    std::size_t files_written = 0;  // files whose bytes changed
    std::size_t files_removed = 0;  // stale pages and pictures of an earlier export
    std::filesystem::path page;    // the page to open
    std::string error;             // UTF-8, when !ok
};

// Parses `hlp`, exports it to `dir` unless dir's stamp already names these bytes and this
// exporter version, and returns the page for `ctx` (page_for_context). Files whose bytes
// are unchanged are not rewritten; stale "NN-*.html" and "bmN.png" files are removed.
// Creates `dir`. Never throws for bad input or I/O; the error is in the result.
ExportResult ensure_export(const std::vector<std::uint8_t>& hlp, const std::filesystem::path& dir,
                           std::uint32_t ctx);

}  // namespace is::help
