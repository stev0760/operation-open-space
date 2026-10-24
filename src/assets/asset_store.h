// The asset store (ADR 0002): typed, lazy access to the original's resources and data
// files over a GameFiles directory. Modules load on first use and stay loaded. Text comes
// back as cp1252 std::string, as the original stores it; the shell converts it at the
// Win32 call (ADR 0003). The readers are in src/formats/, the layouts in docs/re/.
//
// Directories follow the original's INI keys, which M2 reads and passes in:
// - the game directory (Path): INSPACE.EXE, ISPACE.DLL, IGRAPHIC.DLL and <Language>_LANG.DLL
//   [BYTES ISPACE.DLL seg9:0x1a7 seg9:0x23d];
// - the sound directory (SndPath): SND_FX.DLL and <Language>_FX.DLL
//   [BYTES ISPACE.DLL seg9:0x26c seg9:0x286];
// - the ship directory (ShipPath): MASTER.BOD and MASTER.SHP (docs/re/formats.md).
// An empty sound or ship directory means the game directory [DESIGN].
//
// Errors: a missing file or resource throws NotFound; malformed data throws
// is::fmt::FormatError. Not thread-safe.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/game_files.h"
#include "formats/dib.h"
#include "formats/icon.h"
#include "formats/ne.h"
#include "formats/records.h"
#include "formats/res16.h"
#include "formats/ships.h"
#include "formats/version.h"
#include "formats/wav.h"

namespace is::assets {

inline constexpr const char* kGameDll = "ISPACE.DLL";
inline constexpr const char* kGraphicsDll = "IGRAPHIC.DLL";
inline constexpr const char* kMainProgram = "INSPACE.EXE";
// The effects module, in the sound directory [BYTES ISPACE.DLL seg9:0x280].
inline constexpr const char* kSoundDll = "SND_FX.DLL";
inline constexpr const char* kBodyFile = "MASTER.BOD";
inline constexpr const char* kShipFile = "MASTER.SHP";

// IGRAPHIC.DLL's sprite DIBs (docs/re/resources.md#custom-types).
constexpr std::uint16_t kSpriteType = 4000;

// The language when the INI has no Language key, as in the original (ADR 0002, decision 6).
inline constexpr const char* kDefaultLanguage = "ENG";

// The font a converted dialog asks for. The original's dialogs name Helv 8 through
// DS_SETFONT; the port's mapping waits for M2's window and scaling ADR, so it is a
// parameter [DESIGN]. An empty face or a zero size keeps the stored value.
struct DialogFont {
    std::string face;  // cp1252
    std::uint16_t point_size = 0;
};

struct IconGroup {
    std::vector<fmt::IconDirEntry> entries;
    std::vector<fmt::Icon> images;  // the entries' ICON resources, in entry order
    std::vector<std::uint8_t> ico;  // the group as a .ico file (fmt::build_ico)
};

struct Sound {
    std::vector<std::uint8_t> file;  // the RIFF file, cut to its RIFF size
    fmt::Wave wave;                  // parsed from `file`
};

struct Ships {
    fmt::MasterBod bodies;
    fmt::MasterShp ships;

    // The first ship whose name equals `name` (cp1252, exact), or nullptr.
    const fmt::ShipRecord* find(const std::string& name) const;
    // The ship's body record (+0x21 indexes MASTER.BOD), or nullptr when out of range.
    const fmt::BodyRecord* body(const fmt::ShipRecord& ship) const;
};

class AssetStore {
public:
    explicit AssetStore(GameFiles files);
    explicit AssetStore(const std::filesystem::path& dir) : AssetStore(GameFiles(dir)) {}

    const GameFiles& files() const { return files_; }
    GameFiles& files() { return files_; }

    // ---- modules

    // The NE module of the game directory with this installed name (cp1252), loaded on first
    // use. Modules are cached by their file on disk.
    const fmt::NeModule& module(const std::string& name) const;

    // The INI's Language value (M2 reads the INI and calls this only when the key exists).
    void set_language(std::string language) { language_ = std::move(language); }
    const std::string& language() const { return language_; }
    // "<Language>_LANG.DLL", in the game directory [BYTES ISPACE.DLL seg9:0x1e8].
    std::string language_module() const { return language_ + "_LANG.DLL"; }
    const fmt::NeModule& language_dll() const { return module(language_module()); }

    // The INI's SndPath (M2); empty, the default, means the game directory. The directory is
    // listed on first use after the call.
    void set_sound_dir(std::filesystem::path dir);
    const std::filesystem::path& sound_dir() const { return sound_dir_; }
    // The sound directory's files: the sound modules, and the loose WAVs that ordinal 9
    // plays by name (docs/re/ispace.md). Throws NotFound when it is not a readable directory.
    // The reference stays valid until the next set_sound_dir().
    const GameFiles& sound_files() const;
    // "<Language>_FX.DLL", the voice module, in the sound directory
    // [BYTES ISPACE.DLL seg9:0x2c7].
    std::string fx_module() const { return language_ + "_FX.DLL"; }
    // The NE module of the sound directory with this installed name (kSoundDll, fx_module()).
    const fmt::NeModule& sound_module(const std::string& name) const;

    // ---- the language module's resources
    // Resource names (menus, accelerators, bitmaps, icon groups) compare without regard to
    // ASCII case; the modules store them in upper case. An empty name throws NotFound.

    // STRING id. A missing or empty string throws NotFound (LoadString returns 0 for both).
    std::string string(std::uint16_t id) const;
    std::optional<std::string> find_string(std::uint16_t id) const;

    fmt::Dialog16 dialog(std::uint16_t id) const;
    // The Win32 DLGTEMPLATE of DIALOG id, with `font` applied and `or_style` ORed into the
    // dialog style (fmt::dialog_to_win32).
    std::vector<std::uint8_t> dialog_template(std::uint16_t id, const DialogFont& font = {},
                                              std::uint32_t or_style = 0) const;

    std::vector<fmt::MenuItem16> menu(std::uint16_t id) const;
    std::vector<fmt::MenuItem16> menu(const std::string& name) const;
    // The Win32 menu template (fmt::menu_to_win32).
    std::vector<std::uint8_t> menu_template(std::uint16_t id) const;
    std::vector<std::uint8_t> menu_template(const std::string& name) const;

    // ACCELERATOR name (MAINACCEL, FACTACCEL).
    std::vector<fmt::Accel16> accelerators(const std::string& name) const;

    // ---- graphics, icons, sounds, version
    // `module` names a module of the game directory, except for wav().

    // #4000 id: IGRAPHIC.DLL's sprites by default (ISPACE.DLL holds 220, 221 and 300).
    fmt::Dib sprite(std::uint16_t id, const std::string& module = kGraphicsDll) const;
    // A named BITMAP (IGRAPHIC.DLL's ALPHABET, ...; ISPACE.DLL's SD_LOGO).
    fmt::Dib bitmap(const std::string& name, const std::string& module = kGraphicsDll) const;
    IconGroup group_icon(const std::string& module, const std::string& name) const;
    IconGroup group_icon(const std::string& module, std::uint16_t id) const;
    // #100 id of a sound module (kSoundDll or fx_module()), from the sound directory.
    Sound wav(const std::string& module, std::uint16_t id) const;
    // The module's VERSION resource; NotFound when it has none.
    fmt::VersionInfo version(const std::string& module) const;

    // ---- data files and records

    // MASTER.BOD and MASTER.SHP from `ship_dir`, the INI's ShipPath once M2 reads it, or from
    // the game directory when `ship_dir` is empty. The directory is listed and the files read
    // afresh on every call, so files that the Ship Factory (M8) wrote are seen, also where
    // they sit beside the distribution's MASTER.BO1 and MASTER.SH1.
    Ships ships(const std::filesystem::path& ship_dir = {}) const;

    fmt::Palette5500 palette5500() const;  // ISPACE.DLL #5500 100
    fmt::Table5600 table5600() const;      // ISPACE.DLL #5600 135
    fmt::Bytes5000 bytes5000() const;      // ISPACE.DLL #5000 150
    // IGRAPHIC.DLL #6600 id.
    fmt::Scene6600 scene6600(std::uint16_t id,
                             std::size_t granularity = fmt::kWineGlobalGranularity) const;

private:
    const fmt::NeModule& load(const GameFiles& files, const std::string& name) const;
    const fmt::NeResource& need(const std::string& module, std::uint16_t type,
                                std::uint16_t id) const;
    const fmt::NeResource& need(const std::string& module, std::uint16_t type,
                                const std::string& name) const;
    IconGroup icon_group(const std::string& module, const fmt::NeResource& group) const;

    GameFiles files_;
    std::string language_ = kDefaultLanguage;
    std::filesystem::path sound_dir_;
    mutable std::optional<GameFiles> sound_files_;  // listed on first use; empty sound_dir_: unused
    // by the module file's path on disk
    mutable std::map<std::filesystem::path::string_type, std::unique_ptr<fmt::NeModule>> modules_;
};

}  // namespace is::assets
