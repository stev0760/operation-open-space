// WinHelp 3.1 (.HLP) reader, ported from re/isre/hlp.py, the reference this reader matches
// (topics, paragraphs, fonts, links, context ids, keywords, pictures). The manual the
// original opens with WinHelp is <Language>_HELP.HLP in the game directory
// (docs/re/shell.md "Help"); ADR 0005 has the port read it at run time and show it as HTML
// (src/help). Format reference: helpdeco's helpfile.txt [REF helpdeco helpfile.txt], with the
// facts the reference reader states in its module comment, checked against both Inner Space
// manuals (Eng_help.hl1 of 2000 and ENG_HELP.HLP of 1998):
//
// - A 16-byte file header (magic 0x00035F3F, directory offset); every internal file starts
//   with a 9-byte FILEHEADER (reserved, used size, flags). The directory, |CONTEXT,
//   |TTLBTREE and |KWBTREE are B+ trees (magic 0x293B, 38-byte header, pages after it).
// - |SYSTEM: magic 0x036C; minor > 16 means typed records follow (minor 21 = HC31).
//   Windows 3.0 files (minor <= 16) and phrase-compressed text are not supported.
// - |TOPIC is split into 4096-byte blocks (2048 when |SYSTEM flags = 8), each with a 12-byte
//   header. TOPICPOS: block = (pos - 12) / 0x4000, offset = (pos - 12) % 0x4000. Records
//   are walked through NextBlock; the walk ends at the record whose NextBlock is 0 or -1.
// - TOPICOFFSET (|CONTEXT, |CTXOMAP, |TTLBTREE, |KWDATA, browse links) = block * 0x8000 +
//   character count; the count restarts with the first record that starts in a block.
// - Hotspot jumps carry the hash of the context id. The names are not stored in the file:
//   they are recovered by hashing candidate strings (titles, keywords, link texts);
//   unrecovered ids read "#XXXXXXXX".
//
// Text stays cp1252 as stored (ADR 0003). Portable; nothing here writes files.
// Errors: a file that is not a WinHelp 3.1 file throws FormatError; damage inside a topic
// or a picture is listed in HlpFile::errors() and the rest still parses, as in the
// reference.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "formats/bytes.h"

namespace is::fmt {

// The WinHelp 3.1 context-id hash, unsigned 32-bit; the empty name hashes to 1. `name` is
// cp1252. Upper and lower case hash alike.
std::uint32_t hlp_context_hash(const std::string& name);

// WinHelp LZ77 (flag byte LSB first; a set bit is a 2-byte back reference of length
// (w >> 12) + 3 at distance (w & 0xFFF) + 1). `limit` < 0: no limit.
std::vector<std::uint8_t> hlp_lz77_expand(const std::uint8_t* p, std::size_t n, long limit = -1);
// WinHelp picture RunLen: n & 0x80 copies n & 0x7F bytes, else the next byte repeats n times.
std::vector<std::uint8_t> hlp_runlen_expand(const std::uint8_t* p, std::size_t n);

struct HlpWindow {
    std::uint16_t flags = 0;
    std::string type, name, caption;
    std::int16_t x = 0, y = 0, width = 0, height = 0, maximize = 0;
    std::uint32_t rgb = 0, rgb_nsr = 0;  // COLORREF, valid when flags & 0x100 / 0x200
};

struct HlpSystem {
    std::uint16_t minor = 0, major = 0, flags = 0;
    std::uint32_t gen_date = 0;  // seconds since 1970, UTC
    std::string title, copyright, citation;
    std::optional<std::int32_t> contents;  // TOPICOFFSET of the contents topic (record 3)
    std::vector<std::string> config;       // macros run when the file opens (record 4)
    std::vector<HlpWindow> windows;        // record 6
    std::vector<std::uint8_t> icon;        // record 5
    std::map<std::uint16_t, std::vector<std::uint8_t>> other;
};

struct HlpFont {
    std::uint8_t attributes = 0;  // 1 bold, 2 italic, 4 underline, 8 strike-out,
                                  // 0x10 double underline, 0x20 small capitals
    std::uint8_t half_points = 0;
    std::uint8_t family = 0;      // 1 modern, 2 roman, 3 swiss, 4 script, 5 decorative
    std::string face;
    std::uint8_t r = 0, g = 0, b = 0;

    bool bold() const { return attributes & 1; }
    bool italic() const { return attributes & 2; }
};

enum class HlpLinkKind : std::uint8_t { kJump, kPopup, kMacro, kExternal };

struct HlpLink {
    HlpLinkKind kind = HlpLinkKind::kJump;
    std::string target;  // context name, macro text, or name + external reference
    std::optional<std::uint32_t> hash;
};

enum class HlpRunKind : std::uint8_t { kText, kTab, kBreak, kImage, kEmbed };

// A piece of paragraph content. `link` indexes HlpTopic::link_table, -1 for none.
struct HlpRun {
    HlpRunKind kind = HlpRunKind::kText;
    std::string text;  // text; the picture name (bmN) of an image; the embed description
    int font = -1;     // index into HlpFile::fonts()
    int link = -1;
    char align = 0;    // images: 'c' inline (0x86), 'l' left (0x87), 'r' right (0x88)
};

enum class HlpAlign : std::uint8_t { kLeft, kRight, kCenter };

struct HlpParagraph {
    std::vector<HlpRun> runs;
    bool nonscroll = false;
    // Spacing and indents as stored (compressed signed shorts). HC31 stores them in units of
    // 10 twips, half a point, rounded [REF helpdeco helpdeco.c, scaling 10].
    int space_before = 0, space_after = 0, line_spacing = 0;
    int left_indent = 0, right_indent = 0, first_indent = 0;
    std::vector<int> tabs;                  // tab stops as stored (& 0x3FFF)
    std::vector<int> tab_types;             // per stop: 0 left, 1 right, 2 centre
    HlpAlign align = HlpAlign::kLeft;

    // Text, tabs as '\t' and line breaks as '\n' (cp1252).
    std::string plain() const;
};

struct HlpTableRow {
    std::vector<std::vector<HlpParagraph>> cells;
    bool nonscroll = false;

    std::string plain() const;
};

using HlpBlock = std::variant<HlpParagraph, HlpTableRow>;

struct HlpTopic {
    int index = 0;            // 1-based, file order
    std::int32_t number = 0;  // TopicNum of the topic header
    std::int32_t offset = 0;  // TOPICOFFSET of the topic start
    std::int32_t pos = 0;     // TOPICPOS of the topic header record
    std::string title;
    std::vector<std::string> macros;
    std::int32_t browse_prev = -1, browse_next = -1;  // TOPICOFFSET
    std::int32_t nonscroll_pos = -1, scroll_pos = -1;  // TOPICPOS
    std::vector<HlpBlock> blocks;
    std::vector<HlpLink> link_table;     // every link the topic's records created
    std::vector<std::string> contexts;   // recovered context names
    std::vector<std::int32_t> map_ids;   // |CTXOMAP ids
    std::vector<std::string> keywords;
    std::vector<std::string> bitmaps;    // bmN names, first use order

    std::string plain() const;
    // Every paragraph in reading order, table cells included.
    std::vector<const HlpParagraph*> paragraphs() const;
    // The index in link_table of each hotspot used by a run, once, in reading order.
    std::vector<int> links() const;
};

struct HlpPicture {
    std::string name;            // "bm0"
    std::uint8_t ptype = 0;      // 5 DDB, 6 DIB, 8 metafile
    std::uint8_t packing = 0;    // 0 none, 1 RunLen, 2 LZ77, 3 both
    std::uint32_t width = 0, height = 0;
    std::uint32_t bit_count = 0, planes = 1;
    std::uint32_t xdpi = 0, ydpi = 0;
    std::uint32_t colors_used = 0, colors_important = 0;
    std::vector<std::uint8_t> palette;  // RGBQUAD entries
    std::vector<std::uint8_t> bits;     // expanded pixel data
    std::uint32_t hotspot_size = 0;
    std::uint16_t count = 1;            // pictures in this |bm file; the first is read
    std::string note;

    // A Windows BMP file for a DIB or DDB picture, byte for byte as the reference's to_bmp
    // writes it; empty for a metafile or an empty picture.
    std::vector<std::uint8_t> to_bmp() const;
};

struct HlpKeyword {
    std::string text;
    std::vector<std::int32_t> offsets;  // TOPICOFFSET
};

class HlpFile {
public:
    // Parses a whole help file (expanded; SZDD expansion is the caller's, through
    // is::assets::GameFiles or szdd_expand). Throws FormatError for a file that is not a
    // WinHelp 3.1 file or is damaged outside topics and pictures.
    explicit HlpFile(std::vector<std::uint8_t> data);

    const HlpSystem& system() const { return system_; }
    bool lz77() const { return lz77_; }
    int block_size() const { return block_size_; }
    // Internal files in directory order: name and file offset.
    const std::vector<std::pair<std::string, std::int32_t>>& internal_files() const {
        return files_;
    }
    bool has(const std::string& name) const;
    // The data of an internal file (after its FILEHEADER); throws FormatError when missing.
    std::vector<std::uint8_t> file(const std::string& name) const;

    const std::vector<HlpFont>& fonts() const { return fonts_; }
    const HlpFont* font(int i) const;
    const std::vector<std::pair<std::uint32_t, std::int32_t>>& contexts() const {
        return contexts_;
    }  // (hash, TOPICOFFSET)
    const std::vector<std::pair<std::int32_t, std::int32_t>>& ctxomap() const {
        return ctxomap_;
    }  // (map id, TOPICOFFSET)
    const std::map<std::int32_t, std::string>& titles() const { return titles_; }
    const std::vector<HlpKeyword>& keywords() const { return keywords_; }
    const std::map<std::string, HlpPicture>& pictures() const { return pictures_; }
    const std::vector<HlpTopic>& topics() const { return topics_; }
    int topic_headers() const { return topic_headers_; }
    std::optional<std::int32_t> end_offset() const { return end_offset_; }
    const std::vector<std::string>& errors() const { return errors_; }

    // The recovered name of a context hash, or "#XXXXXXXX".
    std::string context_name(std::uint32_t hash) const;
    // The topic holding TOPICOFFSET `offset`; nullptr for a negative or out-of-range offset.
    const HlpTopic* topic_at(std::int32_t offset) const;
    // The topic a context hash names (the first |CONTEXT entry with that hash).
    const HlpTopic* topic_for_hash(std::uint32_t hash) const;
    // The topic of a |CTXOMAP id, as WinHelp(HELP_CONTEXT, id) opens it; nullptr when the
    // map lacks the id.
    const HlpTopic* topic_for_map_id(std::int32_t id) const;
    // The contents topic: |SYSTEM record 3, else the first topic, as WinHelp's HELP_INDEX
    // and Contents button show it [REF Win16 SDK WinHelp]; nullptr without topics.
    const HlpTopic* contents_topic() const;

private:
    std::vector<std::uint8_t> raw_file(std::int32_t off) const;
    template <class Leaf>
    void btree_leaves(const std::vector<std::uint8_t>& b, Leaf&& leaf);
    void parse_system();
    void parse_fonts();
    void parse_context();
    void parse_ctxomap();
    void parse_titles();
    void parse_keywords();
    HlpPicture parse_picture(const std::string& name, const std::vector<std::uint8_t>& b);
    void parse_topics();
    void parse_display(HlpTopic& topic, std::uint8_t rtype, const std::vector<std::uint8_t>& ld1,
                       const std::vector<std::uint8_t>& ld2, bool nonscroll);
    void recover_context_names();
    void attach();

    std::vector<std::uint8_t> data_;
    std::vector<std::pair<std::string, std::int32_t>> files_;
    std::map<std::string, std::int32_t> file_index_;
    HlpSystem system_;
    bool lz77_ = false;
    int block_size_ = 4096;
    std::vector<HlpFont> fonts_;
    std::vector<std::pair<std::uint32_t, std::int32_t>> contexts_;
    std::vector<std::pair<std::int32_t, std::int32_t>> ctxomap_;
    std::map<std::int32_t, std::string> titles_;
    std::vector<HlpKeyword> keywords_;
    std::map<std::string, HlpPicture> pictures_;
    std::vector<HlpTopic> topics_;
    int topic_headers_ = 0;
    std::optional<std::int32_t> end_offset_;
    std::vector<std::int32_t> topic_offsets_;
    std::map<std::uint32_t, std::string> context_names_;
    std::vector<std::string> errors_;
};

}  // namespace is::fmt
