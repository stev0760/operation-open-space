#include "formats/version.h"

#include <algorithm>

namespace is::fmt {

namespace {

constexpr int kMaxDepth = 8;
constexpr std::size_t kFixedSize = 13 * 4;

std::size_t align4(std::size_t p) { return (p + 3) & ~std::size_t{3}; }

// The node at `p`; DWORD alignment counts from the start of the resource. Returns the
// offset just past the node.
std::size_t parse_node(const std::uint8_t* d, std::size_t n, std::size_t p, int depth,
                       VersionNode& node) {
    if (depth > kMaxDepth) throw FormatError("VERSION nesting deeper than 8 levels");
    Reader r(d, n, p);
    const std::size_t length = r.u16();
    const std::size_t vlen = r.u16();
    if (length < 4 || length > n - p) throw FormatError("VERSION node with a bad length");
    const std::size_t end = p + length;
    Reader in(d, end, p + 4);  // the key and value stay inside the node
    node.key = in.cstr();
    std::size_t q = align4(in.pos());
    // An empty value takes no room, so a leaf may end right after its key's NUL, short of
    // the DWORD padding; q can then pass `end` by up to 3 and the loop below adds nothing.
    if (vlen > 0) {
        if (q > end || vlen > end - q) throw FormatError("VERSION value past its node");
        node.value.assign(d + q, d + q + vlen);
    }
    q = align4(q + vlen);
    while (q + 4 <= end) {
        VersionNode child;
        q = align4(parse_node(d, end, q, depth + 1, child));
        node.children.push_back(std::move(child));
    }
    return end;
}

std::string cstr_value(const std::vector<std::uint8_t>& v) {
    const auto nul = std::find(v.begin(), v.end(), 0);
    return std::string(v.begin(), nul);
}

std::string four_part(std::uint32_t ms, std::uint32_t ls) {
    return std::to_string(ms >> 16) + "." + std::to_string(ms & 0xFFFF) + "." +
           std::to_string(ls >> 16) + "." + std::to_string(ls & 0xFFFF);
}

}  // namespace

std::string FixedFileInfo::file_version() const {
    return four_part(file_version_ms, file_version_ls);
}

std::string FixedFileInfo::product_version() const {
    return four_part(product_version_ms, product_version_ls);
}

const std::string* VersionInfo::find_string(const std::string& key) const {
    for (const auto& t : string_tables)
        for (const auto& kv : t.strings)
            if (kv.first == key) return &kv.second;
    return nullptr;
}

VersionInfo parse_version16(const std::uint8_t* p, std::size_t n) {
    VersionInfo v;
    v.size = parse_node(p, n, 0, 0, v.root);
    if (v.root.key != "VS_VERSION_INFO") throw FormatError("VERSION root key is " + v.root.key);

    if (v.root.value.size() >= kFixedSize) {
        Reader r(v.root.value.data(), v.root.value.size());
        FixedFileInfo f;
        for (std::uint32_t* field :
             {&f.signature, &f.struc_version, &f.file_version_ms, &f.file_version_ls,
              &f.product_version_ms, &f.product_version_ls, &f.file_flags_mask, &f.file_flags,
              &f.file_os, &f.file_type, &f.file_subtype, &f.file_date_ms, &f.file_date_ls})
            *field = r.u32();
        if (f.signature != FixedFileInfo::kSignature)
            throw FormatError("bad VS_FIXEDFILEINFO signature");
        v.fixed = f;
    }

    for (const auto& child : v.root.children) {
        if (child.key == "StringFileInfo") {
            for (const auto& table : child.children) {
                VersionStringTable t;
                t.key = table.key;
                for (const auto& s : table.children)
                    t.strings.emplace_back(s.key, cstr_value(s.value));
                v.string_tables.push_back(std::move(t));
            }
        } else if (child.key == "VarFileInfo") {
            for (const auto& var : child.children) {
                std::vector<std::uint16_t> words;
                Reader r(var.value.data(), var.value.size());
                while (r.remaining() >= 2) words.push_back(r.u16());
                v.vars.emplace_back(var.key, std::move(words));
            }
        }
    }
    return v;
}

std::optional<VersionInfo> load_version(const NeModule& m) {
    const auto all = m.all(RT16_VERSION);
    if (all.empty()) return std::nullopt;
    return parse_version16(m.data(*all.front()), m.size(*all.front()));
}

}  // namespace is::fmt
