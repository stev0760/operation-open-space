// isview's model of a game directory's NE modules and its --dump output: a JSON summary of a
// module for comparison with work/assets/<module>/index.json, which `isre res` writes
// (re/isre/res.py build_module). Free of Win32, so that tests/test_isview_dump.cpp compares
// it with index.json; main.cpp includes it, shows the modules and writes the summary.
//
// The keys that both carry hold the same values: module (the lower-case installed name, as
// isre's module key for a shipdisk file), module_name, description, resource_align_shift,
// counts (in the order the types first appear), strings, and per resource type, type_id, id,
// size, file_offset and flags. A named type or id is a string in type_id or id, as in
// index.json. isview adds file, stored_as, installed_size, resource_count, and error for a
// module that does not load.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "assets/asset_store.h"
#include "formats/bytes.h"
#include "formats/ne.h"
#include "formats/res16.h"
#include "text.h"

namespace is::isview {

// ---- resource types and ids

// The names `isre res` gives the integer types (re/isre/ne.py RES_TYPE_NAMES); nullptr for
// any other type, which isre names by its number.
inline const char* std_type_name(std::uint16_t t) {
    switch (t) {
    case 1: return "CURSOR";
    case 2: return "BITMAP";
    case 3: return "ICON";
    case 4: return "MENU";
    case 5: return "DIALOG";
    case 6: return "STRING";
    case 7: return "FONTDIR";
    case 8: return "FONT";
    case 9: return "ACCELERATOR";
    case 10: return "RCDATA";
    case 11: return "MESSAGETABLE";
    case 12: return "GROUP_CURSOR";
    case 14: return "GROUP_ICON";
    case 15: return "NAMETABLE";
    case 16: return "VERSION";
    case 17: return "DLGINCLUDE";
    case 19: return "PLUGPLAY";
    case 20: return "VXD";
    case 21: return "ANICURSOR";
    case 22: return "ANIICON";
    case 23: return "HTML";
    case 24: return "MANIFEST";
    default: return nullptr;
    }
}

inline bool named_type(const fmt::NeResource& r) { return !r.type_name.empty(); }
inline bool named_id(const fmt::NeResource& r) { return !r.name.empty(); }

// index.json's "type": the type's name, its standard name or its number (cp1252).
inline std::string type_label(const fmt::NeResource& r) {
    if (named_type(r)) return r.type_name;
    if (const char* n = std_type_name(r.type)) return n;
    return std::to_string(r.type);
}

inline std::string id_label(const fmt::NeResource& r) {
    return named_id(r) ? r.name : std::to_string(r.id);
}

// ---- modules

// An MZ file whose dword at 0x3C points at an "NE" signature inside the file.
inline bool is_ne(const std::vector<std::uint8_t>& b) {
    if (b.size() < 0x40 || b[0] != 'M' || b[1] != 'Z') return false;
    const std::size_t ne = b[0x3C] | (b[0x3D] << 8) | (b[0x3E] << 16) |
                           (static_cast<std::size_t>(b[0x3F]) << 24);
    return ne <= b.size() - 2 && b[ne] == 'N' && b[ne + 1] == 'E';
}

// The module description: the first name of the nonresident name table, whose file offset
// is the dword at NE +0x2C and whose length is the word at NE +0x20 (re/isre/ne.py). Empty
// when the table is missing or unreadable.
inline std::string ne_description(const std::vector<std::uint8_t>& b) {
    try {
        fmt::Reader r(b.data(), b.size());
        r.seek(0x3C);
        const std::size_t ne = r.u32();
        r.seek(ne + 0x20);
        const std::uint16_t length = r.u16();
        r.seek(ne + 0x2C);
        const std::uint32_t offset = r.u32();
        if (length == 0 || offset == 0) return {};
        r.seek(offset);
        return r.pstr();
    } catch (const fmt::FormatError&) {
        return {};
    }
}

// The non-empty strings of every STRING block, as `isre res` counts them; 0 when a block does
// not decode.
inline std::size_t string_count(const fmt::NeModule& m) {
    try {
        return fmt::all_strings(m).size();
    } catch (const fmt::FormatError&) {
        return 0;
    }
}

struct ModuleInfo {
    std::string name;                   // installed name, cp1252
    std::filesystem::path path;         // the file on disk
    bool distribution_name = false;
    std::size_t size = 0;               // installed size
    std::string description;            // the first entry of the nonresident name table
    const fmt::NeModule* ne = nullptr;  // owned by the asset store
    std::string error;                  // why the module did not load: what(), UTF-8 and cp1252
};

// A path as UTF-16. Native UTF-16 (Windows) converts as it is; native bytes that are not
// UTF-8 (elsewhere) decode as message_to_utf16 does.
inline std::u16string path_utf16(const std::filesystem::path& p) {
    try {
        return p.u16string();
    } catch (const std::exception&) {
        return message_to_utf16(p.string());
    }
}

// Every file of the store's directory that is an NE module, in GameFiles order (by installed
// name). The modules themselves come from the store, which owns them. A file that cannot be
// read, or whose name cp1252 cannot hold, is left out: the identification reports the first.
inline std::vector<ModuleInfo> scan_modules(const assets::AssetStore& store) {
    std::vector<ModuleInfo> out;
    for (const assets::GameFiles::Entry& e : store.files().entries()) {
        const auto name = cp1252_from_utf16(path_utf16(e.name));
        if (!name) continue;
        std::vector<std::uint8_t> bytes;
        try {
            bytes = store.files().read(e);
        } catch (const std::exception&) {
            continue;
        }
        if (!is_ne(bytes)) continue;
        ModuleInfo mi;
        mi.name = *name;
        mi.path = e.path;
        mi.distribution_name = e.distribution_name;
        mi.size = bytes.size();
        mi.description = ne_description(bytes);
        try {
            mi.ne = &store.module(mi.name);
        } catch (const std::exception& x) {
            mi.error = x.what();
        }
        out.push_back(std::move(mi));
    }
    return out;
}

inline bool ascii_ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
        if (x != y) return false;
    }
    return true;
}

// The module with this installed name (ASCII case ignored), or nullptr.
inline const ModuleInfo* find_module(const std::vector<ModuleInfo>& modules, const std::string& name) {
    for (const ModuleInfo& m : modules)
        if (ascii_ieq(m.name, name)) return &m;
    return nullptr;
}

// ---- JSON (UTF-8: the summary is the port's own file, ADR 0003 decision 5)

inline std::string json_string(const std::u16string& s) {
    std::string o = "\"";
    for (char c : utf8_from_utf16(s)) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (u < 0x20) {
            char b[8];
            std::snprintf(b, sizeof b, "\\u%04x", u);
            o += b;
        } else {
            o += c;
        }
    }
    return o + "\"";
}

inline std::string json_cp1252(const std::string& s) { return json_string(fmt::cp1252_to_utf16(s)); }

inline std::string ascii_lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

inline std::string module_json(const ModuleInfo& m) {
    std::string o = "{\n";
    o += " \"module\": " + json_cp1252(ascii_lower(m.name)) + ",\n";
    o += " \"file\": " + json_cp1252(m.name) + ",\n";
    o += " \"stored_as\": " + json_string(path_utf16(m.path.filename())) + ",\n";
    o += " \"installed_size\": " + std::to_string(m.size) + ",\n";
    if (!m.ne) {
        o += " \"error\": " + json_string(message_to_utf16(m.error)) + "\n}";
        return o;
    }
    const fmt::NeModule& ne = *m.ne;
    o += " \"module_name\": " + json_cp1252(ne.module_name()) + ",\n";
    o += " \"description\": " + json_cp1252(m.description) + ",\n";
    o += " \"resource_align_shift\": " + std::to_string(ne.align_shift()) + ",\n";

    // counts per type label, in the order the types first appear in the table
    std::vector<std::pair<std::string, int>> counts;
    for (const fmt::NeResource& r : ne.resources()) {
        const std::string t = type_label(r);
        auto it = std::find_if(counts.begin(), counts.end(),
                               [&](const std::pair<std::string, int>& c) { return c.first == t; });
        if (it == counts.end()) counts.emplace_back(t, 1);
        else ++it->second;
    }
    o += " \"counts\": {";
    for (std::size_t i = 0; i < counts.size(); ++i)
        o += std::string(i ? "," : "") + "\n  " + json_cp1252(counts[i].first) + ": " +
             std::to_string(counts[i].second);
    o += counts.empty() ? "},\n" : "\n },\n";
    o += " \"resource_count\": " + std::to_string(ne.resources().size()) + ",\n";
    o += " \"strings\": " + std::to_string(string_count(ne)) + ",\n";

    o += " \"resources\": [";
    for (std::size_t i = 0; i < ne.resources().size(); ++i) {
        const fmt::NeResource& r = ne.resources()[i];
        char flags[16];
        std::snprintf(flags, sizeof flags, "0x%04x", static_cast<unsigned>(r.flags));
        o += std::string(i ? "," : "") + "\n  {\"type\": " + json_cp1252(type_label(r));
        o += ", \"type_id\": " + (named_type(r) ? json_cp1252(r.type_name) : std::to_string(r.type));
        o += ", \"id\": " + (named_id(r) ? json_cp1252(r.name) : std::to_string(r.id));
        o += ", \"size\": " + std::to_string(r.length);
        o += ", \"file_offset\": " + std::to_string(r.offset);
        o += ", \"flags\": \"" + std::string(flags) + "\"}";
    }
    o += ne.resources().empty() ? "]\n}" : "\n ]\n}";
    return o;
}

// --dump *: every module's summary in one array.
inline std::string modules_json(const std::vector<ModuleInfo>& modules) {
    std::string json = "[";
    for (std::size_t i = 0; i < modules.size(); ++i)
        json += std::string(i ? ",\n" : "\n") + module_json(modules[i]);
    return json + "\n]\n";
}

}  // namespace is::isview
