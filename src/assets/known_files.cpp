#include "assets/known_files.h"

#include <algorithm>
#include <map>

#include "formats/bytes.h"
#include "formats/sha256.h"

namespace is::assets {

namespace fs = std::filesystem;

namespace {
// Rows by name_key of their installed name.
const std::multimap<fs::path::string_type, const KnownFile*>& rows_by_name() {
    static const auto rows = [] {
        std::multimap<fs::path::string_type, const KnownFile*> m;
        for (const KnownFile& k : known_files()) m.emplace(name_key(path_from_cp1252(k.name)), &k);
        return m;
    }();
    return rows;
}
}  // namespace

const char* package_label(Package p) {
    switch (p) {
    case Package::kGame: return "game v1.29.08";
    case Package::kTheWorks: return "theworks";
    }
    return "?";
}

const KnownFile* find_known(const fs::path& name, std::uint64_t size, const std::string& sha256) {
    const auto range = rows_by_name().equal_range(name_key(name));
    for (auto it = range.first; it != range.second; ++it)
        if (it->second->size == size && sha256 == it->second->sha256) return it->second;
    return nullptr;
}

bool known_name(const fs::path& name) { return rows_by_name().count(name_key(name)) != 0; }

std::size_t Identification::unknown_count() const {
    return static_cast<std::size_t>(std::count_if(
        files.begin(), files.end(), [](const FileReport& f) { return f.known == nullptr; }));
}

Identification identify(const GameFiles& files) {
    Identification id;
    id.form = files.form();
    id.shadowed = files.shadowed();
    for (const GameFiles::Entry& e : files.entries()) {
        FileReport f;
        f.path = e.path;
        f.name = e.name;
        f.name_known = known_name(e.name);
        try {
            const std::vector<std::uint8_t> data = files.read(e);
            f.size = data.size();
            f.sha256 = fmt::sha256_hex(data.data(), data.size());
            f.known = find_known(e.name, f.size, f.sha256);
        } catch (const std::exception& x) {  // NotFound or FormatError
            f.error = x.what();
        }
        if (f.known &&
            std::find(id.packages.begin(), id.packages.end(), f.known->package) == id.packages.end())
            id.packages.push_back(f.known->package);
        id.files.push_back(std::move(f));
    }
    std::sort(id.packages.begin(), id.packages.end());
    return id;
}

}  // namespace is::assets
