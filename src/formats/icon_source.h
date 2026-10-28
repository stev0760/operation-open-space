// The port's own icon reader (ADR 0012 decision 5): what SHELL.ExtractIcon gives the original
// for a program file, read from the file's NE or PE resource directory without loading it.
//   - count(): ExtractIcon(path, -1), the number of RT_GROUP_ICON resources; 0 for a file that
//     is not a valid NE or PE module or has none. [BYTES seg6:0x442c seg9:0x178] [REF Win16 SDK
//     ExtractIcon]
//   - cell(i): ExtractIcon(path, i) drawn with DrawIcon into a 32x32 cell blacked with PatBlt
//     BLACKNESS, as seg9:0x1d2/0x1e0 (the wave strip) and seg6:0x44cb/0x44ec (the Navigation
//     preview) draw it. DrawIcon computes (screen AND mask) XOR image, so over black every
//     pixel is the XOR image's colour, masked or not (icon_to_bgra's mask semantics).
//   - is_uniform(): the blank-cell test of seg9:0x8da. [BYTES seg9:0x8da seg9:0x8ef seg9:0x918]
// Portable: no Win32 calls. Callers make the filesystem calls (and log them); load() is the one
// read this file offers, for tools and tests.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "formats/pe.h"

namespace is::fmt {

constexpr int kIconCell = 32;  // the original's cell, whatever SM_CXICON says [BYTES seg9:0x1d2]
// The largest icon member side the reader decodes: an icon directory entry holds the size in a
// byte (0 for 256) [REF Win32 SDK ICONDIRENTRY] [DESIGN ADR 0012] (M3 s1 fixer).
constexpr int kMaxIconSide = 256;

// A 32x32 cell, top-down, 0xFFRRGGBB per pixel (to_bgra's layout; always opaque, since the
// cell is the screen after DrawIcon).
struct IconCell {
    std::array<std::uint32_t, kIconCell * kIconCell> bgra{};
    std::uint32_t at(int x, int y) const { return bgra[static_cast<std::size_t>(y) * kIconCell + x]; }
};

// The display depth the selection fits: the port's display is 8 bpp (ADR 0007 decision 4).
constexpr int kIconDisplayBpp = 8;

class IconSource {
public:
    // Never throws: bytes that do not parse (or exhaust memory) give count() 0.
    explicit IconSource(std::vector<std::uint8_t> bytes);
    // A random-access read of `n` bytes at `offset` into `out`; false when they are not there.
    using ReadAt = std::function<bool(std::uint64_t offset, std::size_t n, std::uint8_t* out)>;
    // The module of a file of `size` bytes, read through `read_at` without reading it whole
    // [DESIGN ADR 0012 decision 5] (M3 s1 fixer): the MZ header and the signature at e_lfanew;
    // for a PE image the headers and section table (at most kMaxPeHeaders bytes) and the raw
    // data of the section that holds the resource directory (at most kMaxPeResources bytes; the
    // other sections are left out, so a resource outside it gives no cell); an NE module of at
    // most kMaxNeImage bytes is read whole. Anything else counts 0. Never throws.
    static IconSource read(std::uint64_t size, const ReadAt& read_at);
    // read() of a file; a file that cannot be read gives count() 0, as ExtractIcon does.
    static IconSource load(const std::filesystem::path& path);
    static constexpr std::uint64_t kMaxPeHeaders = 1u << 20;
    static constexpr std::uint64_t kMaxPeResources = 64u << 20;
    static constexpr std::uint64_t kMaxNeImage = 16u << 20;

    IconSource(IconSource&&) noexcept;
    IconSource& operator=(IconSource&&) noexcept;
    ~IconSource();

    ModuleKind kind() const { return kind_; }
    // ExtractIcon(path, -1). Callers apply the original's max(1, n) rule themselves
    // (seg6:0x4441, nav.md 'File pane and icons').
    int count() const;
    // ExtractIcon(path, index) drawn over black. From the group's members that decode (1, 4, 8,
    // 24 and 32 bpp DIBs of at most kMaxIconSide pixels a side, ranked by their headers and only
    // the chosen one decoded (M3 s1 fixer) [DESIGN ADR 0012]; PNG members are skipped [OPEN])
    // it takes the size nearest 32x32 (sum
    // of the width and height differences; the larger on a tie), then the highest depth not
    // above display_bpp, else the lowest above it, then the first in group order [DESIGN ADR
    // 0012, after REF Win32 SDK LookupIconIdFromDirectoryEx]. Another size is scaled to 32x32 by
    // nearest neighbour [DESIGN ADR 0012]. A 32 bpp member with any non-zero alpha is blended
    // over black by its alpha [DESIGN]. nullopt where ExtractIcon gives no icon: index out of
    // range, or the group or every member unreadable.
    std::optional<IconCell> cell(int index, int display_bpp = kIconDisplayBpp) const;

private:
    struct Impl;
    ModuleKind kind_ = ModuleKind::kNone;
    std::unique_ptr<Impl> impl_;
};

// The cell on the 8 bpp surface: each colour through `nearest`, which the caller supplies from
// the port's palette helpers, e.g. [&](std::uint32_t rgb) { return gfx::nearest_index(
// device, gfx::Rgb::of(rgb)); }, as an 8-bit display resolves a DIB colour to the nearest
// device entry [DESIGN ADR 0007]. Top-down, 32 bytes per row.
std::vector<std::uint8_t> cell_to_index8(const IconCell& cell,
                                         const std::function<int(std::uint32_t rgb)>& nearest);

// seg9:0x8da: true when every pixel of the cell equals the reference pixel, which the original
// reads at strip (0, 0), black margin of the blacked strip (world.md 'Blank cells'). A uniform
// cell gets the program-name placeholder. [BYTES seg9:0x8da seg9:0x8ef seg9:0x918]
bool is_uniform(const IconCell& cell, std::uint32_t reference = 0xFF000000u);
bool is_uniform(const std::vector<std::uint8_t>& index8, std::uint8_t reference = 0);

}  // namespace is::fmt
