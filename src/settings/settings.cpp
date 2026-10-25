// Placeholder so the build links while M2 stage 1 implements the settings layer. Every
// function throws until then.
#include "settings/settings.h"

#include <stdexcept>

namespace is::settings {

namespace {
[[noreturn]] void todo() { throw std::logic_error("settings: not implemented"); }
}  // namespace

IniFile::IniFile(std::filesystem::path path) : path_(std::move(path)) {}
std::optional<std::string> IniFile::get(std::string_view, std::string_view, std::size_t) const { todo(); }
std::string IniFile::get_string(std::string_view, std::string_view, std::string_view, std::size_t) const { todo(); }
int IniFile::get_int(std::string_view, std::string_view, int) const { todo(); }
void IniFile::set(std::string_view, std::string_view, std::string) { todo(); }
void IniFile::set_int(std::string_view, std::string_view, int) { todo(); }
void IniFile::remove(std::string_view, std::string_view) { todo(); }
void IniFile::save() const { todo(); }
std::vector<std::string> IniFile::sections() const { todo(); }
std::vector<std::pair<std::string, std::string>> IniFile::entries(std::string_view) const { todo(); }
IniFile::Section* IniFile::find(std::string_view) { todo(); }
const IniFile::Section* IniFile::find(std::string_view) const { todo(); }

bool regenerate(IniFile&, const std::filesystem::path&, const std::filesystem::path&) { todo(); }

}  // namespace is::settings
