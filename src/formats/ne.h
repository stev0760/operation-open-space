// Win16 NE modules: header, module name and resource table. The port never runs Win16
// code; it reads the original DLLs' resources (ENG_LANG.DLL, IGRAPHIC.DLL, ...) as data.
// Layout: docs/re/resources.md.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "formats/bytes.h"

namespace is::fmt {

enum : std::uint16_t {
    RT16_CURSOR = 1, RT16_BITMAP = 2, RT16_ICON = 3, RT16_MENU = 4, RT16_DIALOG = 5,
    RT16_STRING = 6, RT16_ACCELERATOR = 9, RT16_RCDATA = 10, RT16_GROUP_CURSOR = 12,
    RT16_GROUP_ICON = 14, RT16_VERSION = 16,
};

struct NeResource {
    std::uint16_t type = 0;   // integer type (high bit stripped), 0 when named
    std::string type_name;    // named type, upper case as stored
    std::uint16_t id = 0;     // integer id (high bit stripped), 0 when named
    std::string name;         // named resource
    std::uint32_t offset = 0; // file offset
    std::uint32_t length = 0; // table length, rounded up to the alignment unit
    std::uint16_t flags = 0;
};

class NeModule {
public:
    explicit NeModule(std::vector<std::uint8_t> bytes);
    static NeModule load(const std::filesystem::path& path);

    const std::string& module_name() const { return module_name_; }
    unsigned align_shift() const { return shift_; }
    const std::vector<NeResource>& resources() const { return res_; }

    const NeResource* find(std::uint16_t type, std::uint16_t id) const;
    const NeResource* find(std::uint16_t type, const std::string& name) const;
    std::vector<const NeResource*> all(std::uint16_t type) const;

    // The resource's bytes, clamped to the file; padding included (decoders find the end).
    const std::uint8_t* data(const NeResource& r) const { return bytes_.data() + r.offset; }
    std::size_t size(const NeResource& r) const { return r.length; }

private:
    std::vector<std::uint8_t> bytes_;
    std::string module_name_;
    unsigned shift_ = 0;
    std::vector<NeResource> res_;
};

}  // namespace is::fmt
