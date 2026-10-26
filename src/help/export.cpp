// The HTML export of the manual (ADR 0005). The original shows the file in WinHelp; every
// rendering choice below approximates WinHelp 3.1's display with plain HTML and CSS
// [DESIGN].
#include "help/export.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <system_error>

#include "formats/bytes.h"
#include "formats/dib.h"
#include "formats/sha256.h"
#include "help/png.h"

namespace is::help {

namespace fs = std::filesystem;
using fmt::HlpFile;
using fmt::HlpLinkKind;
using fmt::HlpParagraph;
using fmt::HlpRun;
using fmt::HlpRunKind;
using fmt::HlpTableRow;
using fmt::HlpTopic;

namespace {

std::string utf8(const std::string& cp1252) { return fmt::utf8_from_utf16(fmt::cp1252_to_utf16(cp1252)); }

std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        default: o.push_back(c);
        }
    }
    return o;
}

// cp1252 text in a symbol font as the characters it draws, after the reference's
// _SYMBOL_MAP and _WINGDINGS_MAP; other symbols keep their cp1252 reading [DESIGN].
std::u16string symbol_text(const std::string& s, bool wingdings) {
    static const std::map<std::uint8_t, char16_t> kSymbol = {
        {0xB7, 0x2022}, {0xAE, 0x2192}, {0xAC, 0x2190}, {0xAD, 0x2191}, {0xAF, 0x2193},
        {0xD7, 0x22C5}, {0xB0, 0x00B0}, {0xB4, 0x00D7}, {0xA5, 0x221E}};
    static const std::map<std::uint8_t, char16_t> kWingdings = {
        {0x9F, 0x2022}, {0x6C, 0x25CF}, {0x6E, 0x25A0}, {0xA7, 0x25AA},
        {0xD8, 0x27A2}, {0xE8, 0x2794}, {0xFC, 0x2714}, {0x4A, 0x263A}};
    const auto& table = wingdings ? kWingdings : kSymbol;
    std::u16string out;
    const std::u16string plain = fmt::cp1252_to_utf16(s);
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto it = table.find(static_cast<std::uint8_t>(s[i]));
        out.push_back(it != table.end() ? it->second : plain[i]);
    }
    return out;
}

bool py_space(char c) {
    const auto u = static_cast<std::uint8_t>(c);
    return (u >= 0x09 && u <= 0x0D) || (u >= 0x1C && u <= 0x20) || u == 0xA0;
}

std::string strip(const std::string& s) {
    std::size_t a = 0, z = s.size();
    while (a < z && py_space(s[a])) ++a;
    while (z > a && py_space(s[z - 1])) --z;
    return s.substr(a, z - a);
}

// slugify of the reference's MarkdownWriter: lower case, runs of other characters to '-'.
std::string slugify(const std::string& s) {
    std::string o;
    bool dash = false;
    for (char ch : s) {
        char c = ch;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')) {
            o.push_back(c);
            dash = false;
        } else if (!dash) {
            o.push_back('-');
            dash = true;
        }
    }
    const std::size_t a = o.find_first_not_of('-');
    if (a == std::string::npos) return "topic";
    return o.substr(a, o.find_last_not_of('-') - a + 1);
}

std::string block_plain(const fmt::HlpBlock& b) {
    if (const auto* p = std::get_if<HlpParagraph>(&b)) return p->plain();
    return std::get<HlpTableRow>(b).plain();
}

// Half points (HC31's paragraph unit) as CSS points.
std::string pt(int half_points) {
    char buf[32];
    const int a = half_points < 0 ? -half_points : half_points;
    std::snprintf(buf, sizeof buf, "%s%d%spt", half_points < 0 ? "-" : "", a / 2, a % 2 ? ".5" : "");
    return buf;
}

std::string colour(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
    return buf;
}

std::string colorref(std::uint32_t c) {  // COLORREF 0x00BBGGRR
    return colour(static_cast<std::uint8_t>(c), static_cast<std::uint8_t>(c >> 8),
                  static_cast<std::uint8_t>(c >> 16));
}

// The CSS font list for a face name and a font family [DESIGN].
std::string font_family(const std::string& face, int family) {
    std::string list;
    const std::string f = utf8(face);
    if (!f.empty()) list = "\"" + esc(f) + "\", ";
    if (face == "Helv" || face == "MS Sans Serif") list += "\"MS Sans Serif\", \"Microsoft Sans Serif\", Arial, ";
    if (face == "Tms Rmn") list += "\"Times New Roman\", ";
    switch (family) {
    case 1: return list + "monospace";
    case 2: return list + "serif";
    case 4: return list + "cursive";
    case 5: return list + "fantasy";
    default: return list + "sans-serif";
    }
}

bool browse_buttons(const HlpFile& hf) {
    for (const auto& m : hf.system().config) {
        std::string l;
        for (char c : m) l.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c));
        if (l.find("browsebuttons") != std::string::npos) return true;
    }
    return false;
}

const char* const kPageEnd = "</body>\n</html>\n";

}  // namespace

HtmlManual::HtmlManual(const HlpFile& hf) : hf_(hf) {
    for (const auto& t : hf.topics()) {
        char nn[16];
        std::snprintf(nn, sizeof nn, "%02d-", t.index);
        std::string title = t.title;
        if (title.empty()) {
            for (const auto& b : t.blocks) {
                std::string line = strip(block_plain(b));
                line = strip(line.substr(0, line.find('\n')));
                if (!line.empty()) {
                    title = line.substr(0, 60);
                    break;
                }
            }
        }
        files_.push_back(nn + slugify(title.empty() ? "untitled" : title) + ".html");
    }
}

const std::string& HtmlManual::topic_file(int index) const {
    return files_.at(static_cast<std::size_t>(index - 1));
}

std::string HtmlManual::page_for_context(std::uint32_t ctx) const {
    const HlpTopic* t = nullptr;
    if (ctx != 0) t = hf_.topic_for_map_id(static_cast<std::int32_t>(ctx));  // HELP_CONTEXT
    if (!t) t = hf_.contents_topic();  // HELP_INDEX, or an id the map lacks
    return t ? topic_file(t->index) : kContentsPage;
}

std::string HtmlManual::link_target(const HlpTopic& topic, int link) const {
    if (link < 0 || static_cast<std::size_t>(link) >= topic.link_table.size()) return {};
    const fmt::HlpLink& l = topic.link_table[static_cast<std::size_t>(link)];
    if (l.kind == HlpLinkKind::kMacro || l.kind == HlpLinkKind::kExternal || !l.hash) return {};
    const HlpTopic* t = hf_.topic_for_hash(*l.hash);
    return t ? topic_file(t->index) : std::string();
}

std::string HtmlManual::display_title(const HlpTopic& t) const {
    if (!t.title.empty()) return utf8(t.title);
    for (const auto& b : t.blocks) {
        std::string line = strip(block_plain(b));
        line = strip(line.substr(0, line.find('\n')));
        if (!line.empty()) return utf8(line.substr(0, 60));
    }
    return "Untitled topic";
}

std::string HtmlManual::page_head(const std::string& title) const {
    std::string h = "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n"
                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>";
    h += esc(title);
    const std::string file_title = utf8(hf_.system().title);
    if (!file_title.empty() && file_title != title) h += " - " + esc(file_title);
    h += "</title>\n<link rel=\"stylesheet\" href=\"";
    h += kStylesheet;
    h += "\">\n</head>\n<body>\n";
    return h;
}

// WinHelp 3.1's button bar: Contents, Search, Back and, with BrowseButtons(), << and >>.
// "Topics" opens the export's list of every topic, which WinHelp did not have [DESIGN].
std::string HtmlManual::button_bar(const HlpTopic* t) const {
    const HlpTopic* contents = hf_.contents_topic();
    std::string h = "<nav class=\"bar\">";
    h += "<a class=\"btn\" href=\"" + (contents ? topic_file(contents->index) : std::string(kContentsPage)) +
         "\">Contents</a>";
    h += "<a class=\"btn\" href=\"" + std::string(kKeywordsPage) + "\">Search</a>";
    h += "<a class=\"btn\" href=\"#\" onclick=\"history.back(); return false;\">Back</a>";
    h += "<a class=\"btn\" href=\"" + std::string(kContentsPage) + "\">Topics</a>";
    if (browse_buttons(hf_)) {
        const HlpTopic* prev = t && t->browse_prev != -1 ? hf_.topic_at(t->browse_prev) : nullptr;
        const HlpTopic* next = t && t->browse_next != -1 ? hf_.topic_at(t->browse_next) : nullptr;
        if (prev == t) prev = nullptr;
        if (next == t) next = nullptr;
        h += prev ? "<a class=\"btn\" href=\"" + topic_file(prev->index) + "\" title=\"" +
                        esc(display_title(*prev)) + "\">&lt;&lt;</a>"
                  : "<span class=\"btn off\">&lt;&lt;</span>";
        h += next ? "<a class=\"btn\" href=\"" + topic_file(next->index) + "\" title=\"" +
                        esc(display_title(*next)) + "\">&gt;&gt;</a>"
                  : "<span class=\"btn off\">&gt;&gt;</span>";
    }
    h += "</nav>\n";
    return h;
}

std::string HtmlManual::text_html(const HlpRun& r) const {
    const fmt::HlpFont* f = hf_.font(r.font);
    std::string face;
    if (f)
        for (char c : f->face) face.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c));
    if (face == "symbol" || face == "wingdings")
        return esc(fmt::utf8_from_utf16(symbol_text(r.text, face == "wingdings")));
    return esc(utf8(r.text));
}

// A paragraph's runs. Tabs [DESIGN]:
// - without tab stops of its own, a paragraph keeps its tab characters and the stylesheet
//   sets the browser's tab stops to WinHelp's default, every 0.5 inch (tab-size 36pt);
// - with stops (or a hanging indent, whose left indent is an implicit stop), each line
//   (0x81 breaks) is split at the tabs into cells that reach the next stop: the
//   paragraph's stops, the left indent of a hanging indent, then every 0.5 inch, measured
//   from the left margin as RTF measures them. A cell is an inline block at least as wide
//   as the stop is far, so text that runs past a stop pushes the rest along where WinHelp
//   would move it to the next stop. Right and centre stops (tab_types) align left; the
//   Inner Space manuals use one only on lines without a tab.
std::string HtmlManual::runs_html(const HlpTopic& t, const HlpParagraph& p) const {
    const bool tabbed = (!p.tabs.empty() || p.first_indent < 0) &&
                        std::any_of(p.runs.begin(), p.runs.end(),
                                    [](const HlpRun& r) { return r.kind == HlpRunKind::kTab; });
    std::vector<int> stops = p.tabs;
    if (p.first_indent < 0) stops.push_back(p.left_indent);
    std::sort(stops.begin(), stops.end());
    const auto next_stop = [&](int at) {
        for (int s : stops)
            if (s > at) return s;
        return (at / 72 + 1) * 72;  // default stops every 720 twips
    };

    std::string out;
    // Runs of one cell: spans per font, links around runs that share one.
    const auto cell = [&](std::size_t from, std::size_t to) {
        std::string h;
        int open_link = -1;
        const char* link_close = nullptr;  // "</a>" or "</span>" while a link is open
        int open_font = -2;
        const auto close_font = [&] {
            if (open_font != -2) h += "</span>";
            open_font = -2;
        };
        const auto close_link = [&] {
            close_font();
            if (link_close) h += link_close;
            link_close = nullptr;
            open_link = -1;
        };
        for (std::size_t i = from; i < to; ++i) {
            const HlpRun& r = p.runs[i];
            if (r.link != open_link) {
                close_link();
                if (r.link >= 0) {
                    const fmt::HlpLink& l = t.link_table[static_cast<std::size_t>(r.link)];
                    const std::string target = link_target(t, r.link);
                    if (!target.empty()) {
                        h += std::string("<a class=\"") + (l.kind == HlpLinkKind::kPopup ? "popup" : "jump") +
                             "\" href=\"" + target + "\">";
                        link_close = "</a>";
                    } else {
                        // A macro, an external jump or a context the file lacks: shown, not
                        // followed; neither Inner Space manual holds one [OPEN].
                        const char* kind = l.kind == HlpLinkKind::kMacro      ? "WinHelp macro"
                                           : l.kind == HlpLinkKind::kExternal ? "External jump"
                                                                              : "Missing topic";
                        h += std::string("<span class=\"deadlink\" title=\"") + kind + ": " +
                             esc(utf8(l.target)) + "\">";
                        link_close = "</span>";
                    }
                    open_link = r.link;
                }
            }
            switch (r.kind) {
            case HlpRunKind::kText:
                if (r.font != open_font) {
                    close_font();
                    h += r.font >= 0 ? "<span class=\"f" + std::to_string(r.font) + "\">" : "<span>";
                    open_font = r.font;
                }
                h += text_html(r);
                break;
            case HlpRunKind::kImage: {
                close_font();
                const auto it = hf_.pictures().find(r.text);
                const char* cls = r.align == 'l' ? "il" : r.align == 'r' ? "ir" : "ic";
                if (it != hf_.pictures().end() && !it->second.to_bmp().empty())
                    h += std::string("<img class=\"") + cls + "\" src=\"" + r.text + ".png\" width=\"" +
                         std::to_string(it->second.width) + "\" height=\"" + std::to_string(it->second.height) +
                         "\" alt=\"\">";
                else
                    h += "<span class=\"missing\">[" + esc(r.text) + "]</span>";
                break;
            }
            case HlpRunKind::kEmbed:
                close_font();
                h += "<span class=\"missing\">[" + esc(utf8(r.text)) + "]</span>";
                break;
            case HlpRunKind::kTab:
                if (!tabbed) h += '\t';  // a cell boundary otherwise
                break;
            case HlpRunKind::kBreak: break;
            }
        }
        close_link();
        return h;
    };

    std::size_t line_start = 0;
    bool first_line = true;
    for (std::size_t i = 0; i <= p.runs.size(); ++i) {
        if (i < p.runs.size() && p.runs[i].kind != HlpRunKind::kBreak) continue;
        // runs [line_start, i) form one line
        if (!tabbed) {
            out += cell(line_start, i);
        } else {
            int at = first_line ? p.left_indent + p.first_indent : p.left_indent;
            std::size_t from = line_start;
            for (std::size_t k = line_start; k <= i; ++k) {
                if (k < i && p.runs[k].kind != HlpRunKind::kTab) continue;
                if (k == i) {
                    out += cell(from, k);
                } else {
                    const int stop = next_stop(at);
                    out += "<span class=\"tab\" style=\"min-width:" + pt(stop - at) + "\">" + cell(from, k) +
                           "</span>";
                    at = stop;
                    from = k + 1;
                }
            }
        }
        if (i < p.runs.size()) out += "<br>";
        line_start = i + 1;
        first_line = false;
    }
    return out;
}

std::string HtmlManual::paragraph_html(const HlpTopic& t, const HlpParagraph& p) const {
    std::string style;
    if (p.left_indent) style += "margin-left:" + pt(p.left_indent) + ";";
    if (p.right_indent) style += "margin-right:" + pt(p.right_indent) + ";";
    if (p.first_indent) style += "text-indent:" + pt(p.first_indent) + ";";
    if (p.space_before > 0) style += "margin-top:" + pt(p.space_before) + ";";
    if (p.space_after > 0) style += "margin-bottom:" + pt(p.space_after) + ";";
    if (p.align == fmt::HlpAlign::kCenter) style += "text-align:center;";
    if (p.align == fmt::HlpAlign::kRight) style += "text-align:right;";
    std::string h = "<p class=\"p\"";
    if (!style.empty()) h += " style=\"" + style + "\"";
    h += ">";
    const std::string body = runs_html(t, p);
    // An empty paragraph keeps its line, as WinHelp shows a blank \par.
    h += body.empty() ? "&nbsp;" : body;
    h += "</p>\n";
    return h;
}

std::string HtmlManual::topic_page(const HlpTopic& t) const {
    std::string h = page_head(display_title(t));
    h += "<header class=\"top\">\n" + button_bar(&t);
    std::string nsr, sr;
    bool in_table = false;
    std::string* cur = nullptr;
    const auto end_table = [&] {
        if (in_table) *cur += "</table>\n";
        in_table = false;
    };
    for (const auto& b : t.blocks) {
        const bool nonscroll = std::visit([](const auto& x) { return x.nonscroll; }, b);
        std::string* target = nonscroll ? &nsr : &sr;
        if (target != cur) {
            if (cur) end_table();
            cur = target;
        }
        if (const auto* row = std::get_if<HlpTableRow>(&b)) {
            if (!in_table) *cur += "<table class=\"t\">\n";
            in_table = true;
            *cur += "<tr>";
            for (const auto& c : row->cells) {
                *cur += "<td>";
                for (const auto& para : c) *cur += paragraph_html(t, para);
                *cur += "</td>";
            }
            *cur += "</tr>\n";
        } else {
            end_table();
            *cur += paragraph_html(t, std::get<HlpParagraph>(b));
        }
    }
    if (cur) end_table();
    if (!nsr.empty()) h += "<div class=\"nsr\">\n" + nsr + "</div>\n";
    h += "</header>\n<main class=\"sr\">\n" + sr + "</main>\n";
    return h + kPageEnd;
}

std::string HtmlManual::contents_page() const {
    const std::string title = hf_.system().title.empty() ? std::string("Help") : utf8(hf_.system().title);
    std::string h = page_head(title);
    h += "<header class=\"top\">\n" + button_bar(nullptr) + "</header>\n<main class=\"sr list\">\n";
    h += "<h1>" + esc(title) + "</h1>\n<ol>\n";
    for (const auto& t : hf_.topics())
        h += "<li><a class=\"jump\" href=\"" + topic_file(t.index) + "\">" + esc(display_title(t)) + "</a></li>\n";
    h += "</ol>\n<p><a class=\"jump\" href=\"" + std::string(kKeywordsPage) + "\">Keyword index</a></p>\n";
    if (!hf_.system().copyright.empty()) h += "<p class=\"note\">" + esc(utf8(hf_.system().copyright)) + "</p>\n";
    if (!hf_.system().citation.empty()) h += "<p class=\"note\">" + esc(utf8(hf_.system().citation)) + "</p>\n";
    h += "</main>\n";
    return h + kPageEnd;
}

std::string HtmlManual::keywords_page() const {
    std::string h = page_head("Keyword index");
    h += "<header class=\"top\">\n" + button_bar(nullptr) + "</header>\n<main class=\"sr list\">\n";
    h += "<h1>Keyword index</h1>\n<ul class=\"kw\">\n";
    for (const auto& kw : hf_.keywords()) {
        h += "<li>" + esc(utf8(kw.text));
        std::vector<int> seen;
        std::string links;
        for (auto off : kw.offsets) {
            const HlpTopic* t = hf_.topic_at(off);
            if (!t || std::find(seen.begin(), seen.end(), t->index) != seen.end()) continue;
            seen.push_back(t->index);
            links += std::string(links.empty() ? "" : ", ") + "<a class=\"jump\" href=\"" + topic_file(t->index) +
                     "\">" + esc(display_title(*t)) + "</a>";
        }
        if (!links.empty()) h += ": " + links;
        h += "</li>\n";
    }
    h += "</ul>\n</main>\n";
    return h + kPageEnd;
}

std::string HtmlManual::stylesheet() const {
    // WinHelp 3.1: a white topic window, a grey button bar, green hotspots underlined solid
    // (jumps) or dotted (popups) [REF Win16 SDK WinHelp].
    std::string scroll_bg = "#ffffff", nsr_bg = "#ffffff";
    for (const auto& w : hf_.system().windows) {
        if (w.type != "main" && w.name != "main") continue;
        if (w.flags & 0x100) scroll_bg = colorref(w.rgb);
        if (w.flags & 0x200) nsr_bg = colorref(w.rgb_nsr);
    }
    std::string c =
        "/* The help file's fonts and colours. Written by Inner Space from the help file. */\n"
        ":root { color-scheme: light; }\n"
        "body { margin: 0; background: " + scroll_bg + "; color: #000000;"
        " font: 10pt \"MS Sans Serif\", \"Microsoft Sans Serif\", Arial, sans-serif; }\n"
        ".top { position: sticky; top: 0; z-index: 1; background: " + nsr_bg + "; }\n"
        ".bar { display: flex; flex-wrap: wrap; gap: 4px; padding: 4px; background: #c0c0c0;"
        " border-bottom: 1px solid #808080; }\n"
        ".btn { font: 9pt \"MS Sans Serif\", \"Microsoft Sans Serif\", Arial, sans-serif; color: #000000;"
        " text-decoration: none; padding: 2px 10px; background: #c0c0c0;"
        " border: 2px outset #ffffff; }\n"
        ".btn:active { border-style: inset; }\n"
        ".btn.off { color: #808080; }\n"
        ".nsr { padding: 4px 8px; border-bottom: 1px solid #000000; background: " + nsr_bg + "; }\n"
        ".sr { display: block; padding: 4px 8px 16px; }\n"
        ".p { margin: 0; white-space: pre-wrap; tab-size: 36pt; -moz-tab-size: 36pt; }\n"
        ".tab { display: inline-block; box-sizing: border-box; padding-right: 0.3em;"
        " text-indent: 0; white-space: pre-wrap; }\n"
        "a.jump, a.jump span { color: #008000; text-decoration: underline solid; }\n"
        "a.popup, a.popup span { color: #008000; text-decoration: underline dotted; }\n"
        ".deadlink { text-decoration: underline dashed; }\n"
        ".missing { color: #808080; }\n"
        "img.il { float: left; margin: 0 8px 4px 0; }\n"
        "img.ir { float: right; margin: 0 0 4px 8px; }\n"
        "img.ic { vertical-align: bottom; }\n"
        "table.t { border-collapse: collapse; }\n"
        "table.t td { vertical-align: top; padding: 0 8px 0 0; }\n"
        ".list h1 { font: bold 14pt Arial, sans-serif; color: #00007f; }\n"
        ".list li { margin: 2px 0; }\n"
        ".note { color: #404040; font-size: 9pt; }\n";
    for (std::size_t i = 0; i < hf_.fonts().size(); ++i) {
        const fmt::HlpFont& f = hf_.fonts()[i];
        c += ".f" + std::to_string(i) + " { font-family: " + font_family(f.face, f.family) +
             "; font-size: " + pt(f.half_points) + "; font-weight: " + (f.bold() ? "bold" : "normal") +
             "; font-style: " + (f.italic() ? "italic" : "normal") + "; color: " + colour(f.r, f.g, f.b);
        std::string deco;
        if (f.attributes & 0x04) deco += " underline";
        if (f.attributes & 0x10) deco += " underline double";
        if (f.attributes & 0x08) deco += " line-through";
        if (!deco.empty()) c += "; text-decoration:" + deco;
        if (f.attributes & 0x20) c += "; font-variant: small-caps";
        c += "; }\n";
    }
    return c;
}

std::vector<ExportFile> HtmlManual::render() const {
    std::vector<ExportFile> out;
    const auto add = [&](const std::string& name, const std::string& text) {
        out.push_back({name, std::vector<std::uint8_t>(text.begin(), text.end())});
    };
    for (const auto& t : hf_.topics()) add(topic_file(t.index), topic_page(t));
    add(kContentsPage, contents_page());
    add(kKeywordsPage, keywords_page());
    add(kStylesheet, stylesheet());
    for (const auto& [name, pic] : hf_.pictures()) {
        const auto bmp = pic.to_bmp();
        if (bmp.empty()) continue;
        try {
            out.push_back({name + ".png", png_from_dib(fmt::parse_dib(bmp.data(), bmp.size()))});
        } catch (const fmt::FormatError&) {
            // A picture the DIB reader refuses (a bpp it lacks) is left out; its topic shows
            // the placeholder of a missing picture.
        }
    }
    return out;
}

std::string export_stamp(const std::vector<std::uint8_t>& hlp) {
    return fmt::sha256_hex(hlp.data(), hlp.size()) + " " + std::to_string(kExportVersion) + "\n";
}

namespace {

bool same_bytes(const fs::path& p, const std::vector<std::uint8_t>& want) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::vector<std::uint8_t> have((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return have == want;
}

void write_bytes(const fs::path& p, const std::vector<std::uint8_t>& b) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("cannot write " + fmt::path_utf8(p));
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    if (!f) throw std::runtime_error("cannot write " + fmt::path_utf8(p));
}

// A page or picture name an export writes: "NN-slug.html" or "bmN.png".
bool export_name(const std::string& n) {
    const auto ends = [&](const std::string& s) {
        return n.size() > s.size() && n.compare(n.size() - s.size(), s.size(), s) == 0;
    };
    if (ends(".html")) {
        std::size_t i = 0;
        while (i < n.size() && n[i] >= '0' && n[i] <= '9') ++i;
        if (i < 2 || i >= n.size() || n[i] != '-') return false;
        for (std::size_t k = i; k + 5 < n.size(); ++k)
            if (!((n[k] >= '0' && n[k] <= '9') || (n[k] >= 'a' && n[k] <= 'z') || n[k] == '-')) return false;
        return true;
    }
    if (ends(".png") && n.rfind("bm", 0) == 0) {
        if (n.size() == 6) return false;
        for (std::size_t k = 2; k + 4 < n.size(); ++k)
            if (n[k] < '0' || n[k] > '9') return false;
        return true;
    }
    return false;
}

}  // namespace

ExportResult ensure_export(const std::vector<std::uint8_t>& hlp, const fs::path& dir, std::uint32_t ctx) {
    ExportResult r;
    try {
        const std::string stamp = export_stamp(hlp);
        const HlpFile hf(hlp);
        const HtmlManual manual(hf);
        r.page = dir / manual.page_for_context(ctx);
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) throw std::runtime_error("cannot create " + fmt::path_utf8(dir) + ": " + ec.message());

        bool current = same_bytes(dir / kStampFile, std::vector<std::uint8_t>(stamp.begin(), stamp.end()));
        if (current) {
            std::vector<std::string> needed = {kContentsPage, kKeywordsPage, kStylesheet};
            for (const auto& t : hf.topics()) needed.push_back(manual.topic_file(t.index));
            for (const auto& n : needed)
                if (!fs::is_regular_file(dir / n, ec)) current = false;
        }
        if (current) {
            r.ok = true;
            return r;
        }
        r.regenerated = true;
        fs::remove(dir / kStampFile, ec);  // an interrupted export is redone next time
        std::set<std::string> expected;
        for (const auto& f : manual.render()) {
            expected.insert(f.name);
            if (same_bytes(dir / f.name, f.bytes)) continue;
            write_bytes(dir / f.name, f.bytes);
            ++r.files_written;
        }
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (!e.is_regular_file(ec)) continue;
            const std::string n = e.path().filename().u8string();
            if (export_name(n) && !expected.count(n) && fs::remove(e.path(), ec)) ++r.files_removed;
        }
        write_bytes(dir / kStampFile, std::vector<std::uint8_t>(stamp.begin(), stamp.end()));
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = e.what();
    }
    return r;
}

}  // namespace is::help
