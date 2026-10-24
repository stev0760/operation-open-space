#include "assets/asset_store.h"

#include <algorithm>
#include <utility>

#include "formats/bytes.h"

namespace is::assets {

namespace fs = std::filesystem;

namespace {

// A type in a message: its standard name (fmt::resource_type_name, as isview shows it), else
// "#number" as docs/ cite a custom type (IGRAPHIC.DLL #4000 60).
std::string type_label(std::uint16_t type) {
    if (const char* n = fmt::resource_type_name(type)) return n;
    return "#" + std::to_string(type);
}

}  // namespace

const fmt::ShipRecord* Ships::find(const std::string& name) const {
    for (const auto& s : ships.records)
        if (s.name == name) return &s;
    return nullptr;
}

const fmt::BodyRecord* Ships::body(const fmt::ShipRecord& ship) const {
    return ship.body < bodies.records.size() ? &bodies.records[ship.body] : nullptr;
}

AssetStore::AssetStore(GameFiles files) : files_(std::move(files)) {}

const fmt::NeModule& AssetStore::load(const GameFiles& files, const std::string& name) const {
    const GameFiles::Entry* e = files.find(name);
    if (!e) throw NotFound(name + ": no such file in " + fmt::path_utf8(files.dir()));
    auto it = modules_.find(e->path.native());
    if (it == modules_.end()) {
        std::unique_ptr<fmt::NeModule> m;
        try {
            m = std::make_unique<fmt::NeModule>(files.read(*e));
        } catch (const fmt::FormatError& x) {
            throw fmt::FormatError(name + ": " + x.what());
        }
        it = modules_.emplace(e->path.native(), std::move(m)).first;
    }
    return *it->second;
}

const fmt::NeModule& AssetStore::module(const std::string& name) const {
    return load(files_, name);
}

void AssetStore::set_sound_dir(fs::path dir) {
    sound_dir_ = std::move(dir);
    sound_files_.reset();
}

const GameFiles& AssetStore::sound_files() const {
    if (sound_dir_.empty()) return files_;
    if (!sound_files_) sound_files_.emplace(sound_dir_);
    return *sound_files_;
}

const fmt::NeModule& AssetStore::sound_module(const std::string& name) const {
    return load(sound_files(), name);
}

const fmt::NeResource& AssetStore::need(const std::string& module_name, std::uint16_t type,
                                        std::uint16_t id) const {
    const fmt::NeResource* r = module(module_name).find(type, id);
    if (!r) throw NotFound(module_name + ": no " + type_label(type) + " " + std::to_string(id));
    return *r;
}

const fmt::NeResource& AssetStore::need(const std::string& module_name, std::uint16_t type,
                                        const std::string& name) const {
    // NeModule::find(type, "") would match the first resource with an integer id
    if (name.empty())
        throw NotFound(module_name + ": no " + type_label(type) + " with an empty name");
    // the first resource of the type whose name has the same fmt::fold_name, as NeModule::find
    // takes the first exact match
    const std::string key = fmt::fold_name(name);
    for (const fmt::NeResource& r : module(module_name).resources())
        if (r.type == type && r.type_name.empty() && !r.name.empty() && fmt::fold_name(r.name) == key)
            return r;
    throw NotFound(module_name + ": no " + type_label(type) + " " + name);
}

// ---- the language module

std::optional<std::string> AssetStore::find_string(std::uint16_t id) const {
    return fmt::load_string(language_dll(), id);
}

std::string AssetStore::string(std::uint16_t id) const {
    auto s = find_string(id);
    if (!s) throw NotFound(language_module() + ": no STRING " + std::to_string(id));
    return std::move(*s);
}

fmt::Dialog16 AssetStore::dialog(std::uint16_t id) const {
    const fmt::NeResource& r = need(language_module(), fmt::RT16_DIALOG, id);
    const fmt::NeModule& m = language_dll();
    return fmt::parse_dialog16(m.data(r), m.size(r));
}

std::vector<std::uint8_t> AssetStore::dialog_template(std::uint16_t id, const DialogFont& font,
                                                      std::uint32_t or_style) const {
    fmt::Dialog16 d = dialog(id);
    if (!font.face.empty()) {
        d.face = font.face;
        d.style |= fmt::kDsSetFont;  // a dialog stored without a font takes this one too
    }
    if (font.point_size != 0) d.point_size = font.point_size;
    return fmt::dialog_to_win32(d, or_style);
}

std::vector<fmt::MenuItem16> AssetStore::menu(std::uint16_t id) const {
    const fmt::NeResource& r = need(language_module(), fmt::RT16_MENU, id);
    const fmt::NeModule& m = language_dll();
    return fmt::parse_menu16(m.data(r), m.size(r));
}

std::vector<fmt::MenuItem16> AssetStore::menu(const std::string& name) const {
    const fmt::NeResource& r = need(language_module(), fmt::RT16_MENU, name);
    const fmt::NeModule& m = language_dll();
    return fmt::parse_menu16(m.data(r), m.size(r));
}

std::vector<std::uint8_t> AssetStore::menu_template(std::uint16_t id) const {
    return fmt::menu_to_win32(menu(id));
}

std::vector<std::uint8_t> AssetStore::menu_template(const std::string& name) const {
    return fmt::menu_to_win32(menu(name));
}

std::vector<fmt::Accel16> AssetStore::accelerators(const std::string& name) const {
    const fmt::NeResource& r = need(language_module(), fmt::RT16_ACCELERATOR, name);
    const fmt::NeModule& m = language_dll();
    return fmt::parse_accel16(m.data(r), m.size(r));
}

// ---- graphics, icons, sounds, version

fmt::Dib AssetStore::sprite(std::uint16_t id, const std::string& module_name) const {
    const fmt::NeResource& r = need(module_name, kSpriteType, id);
    const fmt::NeModule& m = module(module_name);
    return fmt::parse_dib(m.data(r), m.size(r));
}

fmt::Dib AssetStore::bitmap(const std::string& name, const std::string& module_name) const {
    const fmt::NeResource& r = need(module_name, fmt::RT16_BITMAP, name);
    const fmt::NeModule& m = module(module_name);
    return fmt::parse_dib(m.data(r), m.size(r));
}

IconGroup AssetStore::icon_group(const std::string& module_name, const fmt::NeResource& group) const {
    const fmt::NeModule& m = module(module_name);
    IconGroup g;
    g.entries = fmt::parse_group_icon(m.data(group), m.size(group));
    for (const fmt::IconDirEntry& e : g.entries) {
        const fmt::NeResource* r = m.find(fmt::RT16_ICON, e.id);
        if (!r)
            throw fmt::FormatError(module_name + ": icon group member " + std::to_string(e.id) +
                                   " is missing");
        g.images.push_back(fmt::parse_icon(m.data(*r), m.size(*r)));
    }
    g.ico = fmt::build_ico(m, group);
    return g;
}

IconGroup AssetStore::group_icon(const std::string& module_name, const std::string& name) const {
    return icon_group(module_name, need(module_name, fmt::RT16_GROUP_ICON, name));
}

IconGroup AssetStore::group_icon(const std::string& module_name, std::uint16_t id) const {
    return icon_group(module_name, need(module_name, fmt::RT16_GROUP_ICON, id));
}

Sound AssetStore::wav(const std::string& module_name, std::uint16_t id) const {
    const fmt::NeModule& m = sound_module(module_name);
    const fmt::NeResource* r = m.find(fmt::kWavResourceType, id);
    if (!r)
        throw NotFound(module_name + ": no " + type_label(fmt::kWavResourceType) + " " +
                       std::to_string(id));
    Sound s;
    s.file = fmt::wav_file(m, *r);
    s.wave = fmt::parse_wav(s.file.data(), s.file.size());
    return s;
}

fmt::VersionInfo AssetStore::version(const std::string& module_name) const {
    auto v = fmt::load_version(module(module_name));
    if (!v) throw NotFound(module_name + ": no VERSION resource");
    return std::move(*v);
}

// ---- data files and records

Ships AssetStore::ships(const fs::path& ship_dir) const {
    // a fresh listing: files_ was listed before the Ship Factory could add MASTER.BOD and
    // MASTER.SHP beside MASTER.BO1 and MASTER.SH1
    const GameFiles g(ship_dir.empty() ? files_.dir() : ship_dir);
    const auto bod = g.read(kBodyFile);
    const auto shp = g.read(kShipFile);
    return Ships{fmt::parse_master_bod(bod.data(), bod.size()),
                 fmt::parse_master_shp(shp.data(), shp.size())};
}

// The fmt::load_* readers throw FormatError for a missing record; need() first turns that
// case into NotFound.
fmt::Palette5500 AssetStore::palette5500() const {
    need(kGameDll, fmt::Palette5500::kType, fmt::Palette5500::kId);
    return fmt::load_palette5500(module(kGameDll));
}

fmt::Table5600 AssetStore::table5600() const {
    need(kGameDll, fmt::Table5600::kType, fmt::Table5600::kId);
    return fmt::load_table5600(module(kGameDll));
}

fmt::Bytes5000 AssetStore::bytes5000() const {
    need(kGameDll, fmt::Bytes5000::kType, fmt::Bytes5000::kId);
    return fmt::load_bytes5000(module(kGameDll));
}

fmt::Scene6600 AssetStore::scene6600(std::uint16_t id, std::size_t granularity) const {
    need(kGraphicsDll, fmt::Scene6600::kType, id);
    return fmt::load_scene6600(module(kGraphicsDll), id, granularity);
}

}  // namespace is::assets
