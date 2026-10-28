// Win32 PE modules: the DOS header, the PE signature, the COFF header, the optional header's
// resource data directory, the section table and the resource directory tree (type, name,
// language). Enough to list RT_GROUP_ICON and RT_ICON resources by id and read their bytes for
// the port's own icon reader (ADR 0012 decision 5): the port never loads or runs a user program.
// Portable and bounds-checked; a header or directory that does not parse throws FormatError.
// [REF Microsoft PE/COFF specification, "The .rsrc Section"] [DESIGN ADR 0012]
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "formats/bytes.h"

namespace is::fmt {

// The Win32 type ids equal the Win16 ones (formats/ne.h RT16_*). [REF Win32 SDK winuser.h]
enum : std::uint16_t { RTPE_ICON = 3, RTPE_GROUP_ICON = 14 };

struct PeSection {
    std::string name;  // up to 8 bytes, NULs stripped
    std::uint32_t virtual_address = 0;
    std::uint32_t virtual_size = 0;
    std::uint32_t raw_offset = 0;  // PointerToRawData
    std::uint32_t raw_size = 0;    // SizeOfRawData
};

// One leaf of the resource tree. A named type or name has id 0 and the UTF-16 name; an integer
// one has an empty name.
struct PeResource {
    std::uint16_t type = 0;
    std::u16string type_name;
    std::uint16_t id = 0;
    std::u16string name;
    std::uint16_t lang = 0;
    std::uint32_t rva = 0;   // IMAGE_RESOURCE_DATA_ENTRY.OffsetToData
    std::uint32_t size = 0;
    std::uint32_t codepage = 0;
};

class PeModule {
public:
    // Throws FormatError when the image is not MZ + "PE\0\0" with a parsable optional header,
    // section table and resource directory.
    explicit PeModule(std::vector<std::uint8_t> bytes);
    static PeModule load(const std::filesystem::path& path);

    std::uint16_t machine() const { return machine_; }
    bool pe32_plus() const { return pe32_plus_; }
    const std::vector<PeSection>& sections() const { return sections_; }

    // The leaves in directory order: types, then names, then languages, each level as stored
    // (named entries before id entries, as the image sorts them).
    const std::vector<PeResource>& resources() const { return res_; }
    // The first leaf (first language) of an integer type and integer id, or of a named one.
    const PeResource* find(std::uint16_t type, std::uint16_t id) const;
    const PeResource* find(std::uint16_t type, const std::u16string& name) const;
    // The first leaf of every distinct name under an integer type, in directory order: what
    // ExtractIcon indexes for RT_GROUP_ICON. [REF Win32 SDK ExtractIcon]
    std::vector<const PeResource*> all(std::uint16_t type) const;

    // The file offset of an RVA, through the section table. Throws FormatError when no section's
    // raw data holds [rva, rva + size).
    std::size_t file_offset(std::uint32_t rva, std::uint32_t size) const;
    // A resource's bytes in the file. Throws FormatError when they are not in the file.
    const std::uint8_t* data(const PeResource& r) const { return bytes_.data() + file_offset(r.rva, r.size); }
    std::size_t size(const PeResource& r) const { return r.size; }

private:
    void read_resources(std::uint32_t rva, std::uint32_t size);

    std::vector<std::uint8_t> bytes_;
    std::uint16_t machine_ = 0;
    bool pe32_plus_ = false;
    std::vector<PeSection> sections_;
    std::vector<PeResource> res_;
};

// The signature at the MZ header's e_lfanew: "NE" or "PE\0\0"; kNone for anything else,
// including a file too short to hold one. Never throws.
enum class ModuleKind { kNone, kNe, kPe };
ModuleKind module_kind(const std::uint8_t* p, std::size_t n);

}  // namespace is::fmt
