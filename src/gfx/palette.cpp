#include "gfx/palette.h"

#include <algorithm>
#include <cstdlib>

namespace is::gfx {

namespace {
// 16-bit word arithmetic, as ISPACE.DLL computes.
int w16(long v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff)); }

// DGROUP:0x178[kind], 0x17c[kind], 0x180[kind]: the first entries of ordinal 14's and
// seg5:0x26a2's ranges. Kind 0: 0x2e4, 0x354, 0x398 (entries 185, 213, 230)
// [BYTES ISPACE.DLL DGROUP:0x178 DGROUP:0x17c DGROUP:0x180]; kind 1: entries 10, 83 and 112,
// the ranges shell.md gives for ordinal 14 on other kinds. Kinds above 1 use kind 1's [DESIGN].
struct Bases {
    int ramp, grey, top;
};
Bases bases(int kind) { return kind == 0 ? Bases{185, 213, 230} : Bases{10, 83, 112}; }

// The fixed 16-colour table seg5:0x26a2 substitutes [BYTES ISPACE.DLL seg5:0x270b seg5:0x272b].
constexpr std::uint32_t kFixed16[16] = {0x00ff00, 0x008000, 0x808000, 0xffff00, 0xff0000, 0x800000,
                                        0x800080, 0xff00ff, 0x0000ff, 0x000080, 0x008080, 0x00ffff,
                                        0xffffff, 0xc0c0c0, 0x808080, 0x000000};

// seg6:0x7f52: three Newton steps of an integer square root from a guess (long arithmetic,
// truncating division) [BYTES ISPACE.DLL seg6:0x7f52].
long isqrt3(long guess, long target) {
    long x = guess;
    for (int i = 0; i < 3; ++i) {
        if (x < 2) x = 2;
        x += (target - x * x) / (2 * x);
    }
    return x;
}
}  // namespace

DeviceColours default_device_colours() {
    DeviceColours d{};
    for (int i = 0; i < 10; ++i) d[i] = Rgb::of(kStaticColours[i]);
    for (int i = 0; i < 10; ++i) d[246 + i] = Rgb::of(kStaticColours[10 + i]);
    return d;
}

bool is_static_entry(int i) { return (i >= 0 && i < 10) || (i >= 246 && i < 256); }

int ramp_row(int c) {
    switch (c) {  // [BYTES ISPACE.DLL seg5:0x6292]
    case 3: return 15;
    case 5: return 1;
    case 6: return 3;
    case 7: return 5;
    case 8: return 7;
    case 9: return 17;
    case 10: return 13;
    case 11: return 14;
    case 12: return 6;
    case 13: return 2;
    default: return -1;  // c = 4 and c outside 3-13 leave the row unset [BYTES seg5:0x62ad]
    }
}

int nearest_index(const DeviceColours& pal, Rgb c, int first, int last) {
    int best = first;
    long best_d = -1;
    for (int i = first; i <= last; ++i) {
        const long dr = static_cast<long>(pal[i].r) - c.r, dg = static_cast<long>(pal[i].g) - c.g,
                   db = static_cast<long>(pal[i].b) - c.b;
        const long d = dr * dr + dg * dg + db * db;
        if (best_d < 0 || d < best_d) {
            best_d = d;
            best = i;
            if (d == 0) break;
        }
    }
    return best;
}

int colour_index(std::uint32_t colorref, const DeviceColours& device) {
    if ((colorref >> 16) == 0x10ff) return static_cast<int>(colorref & 0xff);  // DIBINDEX
    const Rgb c{static_cast<std::uint8_t>(colorref), static_cast<std::uint8_t>(colorref >> 8),
                static_cast<std::uint8_t>(colorref >> 16)};  // COLORREF is 0x00BBGGRR
    switch (colorref >> 24) {
    case 1: return static_cast<int>(colorref & 0xff);  // PALETTEINDEX
    case 2: return nearest_index(device, c);          // PALETTERGB
    default: {
        // RGB: the nearest static colour
        const int lo = nearest_index(device, c, 0, 9), hi = nearest_index(device, c, 246, 255);
        const auto dist = [&](int i) {
            const long dr = static_cast<long>(device[i].r) - c.r,
                       dg = static_cast<long>(device[i].g) - c.g,
                       db = static_cast<long>(device[i].b) - c.b;
            return dr * dr + dg * dg + db * db;
        };
        return dist(hi) < dist(lo) ? hi : lo;
    }
    }
}

PaletteManager::PaletteManager()
    : device_(default_device_colours()), schemes10_(kSchemes10), schemes5_(kSchemes5) {
    for (auto& m : scheme_record_) m.fill(-1);
}

int PaletteManager::slot_of(PalHandle pal) const {
    if (pal == 0) return -1;
    for (int i = 0; i < kSlots; ++i)
        if (slots_[i].handle == pal) return i;
    return -1;
}

const PaletteArray* PaletteManager::working(PalHandle pal) const {
    const int s = slot_of(pal);
    return s < 0 ? nullptr : &slots_[s].working;
}
const PaletteArray* PaletteManager::stored(PalHandle pal, int set) const {
    const int s = slot_of(pal);
    return s < 0 || set < 0 || set > 1 ? nullptr : &slots_[s].stored[set];
}
int PaletteManager::kind(PalHandle pal) const {
    const int s = slot_of(pal);
    return s < 0 ? -1 : slots_[s].kind;
}
int PaletteManager::mode(PalHandle pal) const {
    const int s = slot_of(pal);
    return s < 0 ? -1 : slots_[s].mode;
}
PaletteManager::Ramp PaletteManager::ramp_record(int mode, int record) const {
    if (mode < 0 || mode >= kModes || record < 0 || record >= kRecords) return {};
    return records_[mode][record];
}
int PaletteManager::ramp_of(int mode, int c) const {
    if (mode < 0 || mode >= kModes || c < 0 || c >= kSchemeIds) return -1;
    return scheme_record_[mode][c];
}
int PaletteManager::ramp_uses(int mode, int record) const {
    if (mode < 0 || mode >= kModes || record < 0 || record >= kRecords) return 0;
    return record_uses_[mode][record];
}

void PaletteManager::publish(int slot, int first, int count) {
    for (int i = first; i < first + count; ++i) device_[i] = slots_[slot].working[i].rgb();
    ++generation_;
    if (on_publish_) on_publish_(first, count);
}

int PaletteManager::command(PalHandle pal, int a, int b, int c, int cmd) {
    switch (cmd) {  // the jump table at seg5:0x3578 [BYTES ISPACE.DLL seg5:0x3573]
    case 0: return create(a);
    case 1: load_scheme(pal, c, b); return 0;
    case 2: destroy(pal); return 0;
    case 3: animate(pal); return 0;
    case 4: save_set(pal, b, c); return 0;
    case 5: restore_set(pal, b); return 0;
    case 6: return allocate_colour(pal, a, b, c);
    case 7: allocate_ramp(pal, c); return 0;
    case 8: swap_black(pal, c); return 0;
    default: return 0;
    }
}

// Command 0, seg5:0x3596 [BYTES ISPACE.DLL seg5:0x35a5-0x39e5].
PalHandle PaletteManager::create(int kind) {
    int s = 0;
    while (s < kSlots && slots_[s].handle != 0) ++s;
    if (s == kSlots) return 0;  // no free slot: returns 0
    Slot& sl = slots_[s];
    sl.kind = kind;  // DGROUP:0x1e92
    // SIZEPALETTE 256 and NUMRESERVED 20: max(BITSPIXEL, PLANES) is 8, not 4, so system entries
    // 0..9 go to entries 0..9 and system entries 246..255 to entries 246..255, keeping the flags
    // GDI returns (0 for the static colours) [DESIGN].
    const int half = kNumReserved / 2;
    const DeviceColours stat = default_device_colours();
    for (int i = 0; i < half; ++i) {
        sl.working[i] = PalEntry{stat[i].r, stat[i].g, stat[i].b, 0};
        const int j = kSizePalette - half + i;
        sl.working[256 - half + i] = PalEntry{stat[j].r, stat[j].g, stat[j].b, 0};
    }
    if (alphabet_refs_ == 0 && on_alphabet_) on_alphabet_(true);  // seg5:0x3712 while 0x1ea2 = 0
    ++alphabet_refs_;                                             // DGROUP:0x1f3c
    // Entries 10-245 black with PC_RESERVED: the count 236 is a constant [BYTES seg5:0x3880].
    for (int i = kFirstGameEntry; i < kFirstGameEntry + kGameEntries; ++i)
        sl.working[i] = PalEntry{0, 0, 0, kPcReserved};
    sl.handle = ++next_handle_ ? next_handle_ : ++next_handle_;  // CreatePalette
    sl.mode = 0;
    sl.stored = {sl.working, sl.working};  // the two stored copies [BYTES seg5:0x39b8]
    // seg5:0x5846 refills both colour tables, so row 12 is zero again [BYTES seg5:0x39dd].
    schemes10_ = kSchemes10;
    schemes5_ = kSchemes5;
    // The port realises the new palette at once (ADR 0007 decision 5) [DESIGN].
    publish(s, 0, 256);
    return sl.handle;
}

// Command 1, seg5:0x44ac [BYTES ISPACE.DLL seg5:0x44c8-0x4731].
void PaletteManager::load_scheme(PalHandle pal, int c, int mode) {
    const int s = slot_of(pal);
    if (s < 0) return;  // the original stores the mode in DGROUP:0x1e9c[3] = DGROUP:0x1ea2 first
    Slot& sl = slots_[s];
    sl.mode = mode;  // DGROUP:0x1e9c
    if (mode != 2 && mode >= 0 && mode < kModes) {
        scheme_record_[mode].fill(-1);
        for (int r = 0; r < 14; ++r) {  // records 5-18: entries 103 + 10k .. 112 + 10k
            record_uses_[mode][5 + r] = 0;
            records_[mode][5 + r] = {103 + 10 * r, 112 + 10 * r};
        }
        for (int r = 0; r < 5; ++r) {  // records 0-4 belong to schemes 0-4
            scheme_record_[mode][r] = r;
            record_uses_[mode][r] = 1;
        }
        records_[mode][0] = {11, 27};   // blue
        records_[mode][1] = {28, 62};   // yellow-orange-brown
        records_[mode][2] = {73, 89};   // grey
        records_[mode][3] = {63, 72};   // khaki
        records_[mode][4] = {90, 102};  // green
    }
    // At 8 bpp and above [BYTES seg5:0x45c6 seg5:0x45d0 seg5:0x45f8]:
    if (c == -1000) {
        for (int i = 10; i < 246; ++i) sl.working[i].set({255, 255, 255});
    } else {
        const int n = c >= 0 ? 236 : 173;  // 10-245, or 10-182 for other negative c
        for (int i = 10; i < 10 + n; ++i) sl.working[i].set({});
    }
    if (sl.kind == 0 && c != -1000)  // seg5:0x254c, seg5:0x2626: #5500 100 into 11-102
        for (std::size_t i = 0; i < res5500_.size(); ++i) sl.working[11 + i].set(res5500_[i]);
    if (c >= 100 || c == -2) {  // blacks 73-182 [BYTES seg5:0x4656]
        for (int i = 73; i < 183; ++i) sl.working[i].set({});
        c -= 100;
    }
    if (sl.kind == 0 && c > 4) {
        ramp(s, c + 100);
        if (mode == 0)
            for (int k = 5; k < 14; ++k)
                if (k != c) ramp(s, k + 100);
    }
}

// Command 2, seg5:0x3aae [BYTES ISPACE.DLL seg5:0x3aae seg5:0x3b95].
void PaletteManager::destroy(PalHandle pal) {
    const int s = slot_of(pal);
    if (s < 0) return;
    slots_[s] = Slot{};
    if (--alphabet_refs_ == 0 && on_alphabet_) on_alphabet_(false);
}

// Command 3, seg5:0x4378: AnimatePalette(hPal, 10, 236, &entries[10]) [BYTES seg5:0x441a].
void PaletteManager::animate(PalHandle pal) {
    const int s = slot_of(pal);
    if (s < 0) return;
    publish(s, kFirstGameEntry, kGameEntries);
}

// Command 4, seg5:0x484a: RGB of entries 10-245 into stored set b, blacked when c != 0.
void PaletteManager::save_set(PalHandle pal, int set, int blank) {
    const int s = slot_of(pal);
    if (s < 0 || set < 0 || set > 1) return;
    Slot& sl = slots_[s];
    for (int i = 10; i < 246; ++i) {
        sl.stored[set][i].set(sl.working[i].rgb());
        if (blank != 0) sl.working[i].set({});
    }
    animate(pal);
}

// Command 5, seg5:0x474c: stored set b back into entries 10-245.
void PaletteManager::restore_set(PalHandle pal, int set) {
    const int s = slot_of(pal);
    if (s < 0 || set < 0 || set > 1) return;
    Slot& sl = slots_[s];
    for (int i = 10; i < 246; ++i) sl.working[i].set(sl.stored[set][i].rgb());
    animate(pal);
}

// Command 6, seg5:0x3dee [BYTES ISPACE.DLL seg5:0x3e0d-0x4284].
int PaletteManager::allocate_colour(PalHandle pal, int r, int g, int b) {
    const int s = slot_of(pal);
    if (s < 0) return -1;
    Slot& sl = slots_[s];
    const bool up = sl.kind == 0;
    const int first = up ? 10 : 245, last = up ? 244 : 11;
    for (int i = first; up ? i <= last : i >= last; i += up ? 1 : -1) {
        PalEntry& e = sl.working[i];
        if (std::abs(e.r - r) < 4 && std::abs(e.g - g) < 4 && std::abs(e.b - b) < 4) return -1;
        const bool free = e.black() || (e.r == 1 && e.g == 11 && e.b == 111);
        if (free && i != first) {
            e.set({static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                   static_cast<std::uint8_t>(b)});
            return i;
        }
    }
    return -1;
}

// Command 7, seg5:0x446e.
void PaletteManager::allocate_ramp(PalHandle pal, int c) {
    const int s = slot_of(pal);
    if (s < 0) return;
    ramp(s, c);
}

// seg5:0x61b2(slot, c) [BYTES ISPACE.DLL seg5:0x61c1-0x6492].
void PaletteManager::ramp(int s, int c) {
    Slot& sl = slots_[s];
    if (c == -1 || sl.kind == 1) return;
    if (c > 99) c -= 100;
    const int m = sl.mode;
    if (m < 0 || m >= kModes) return;  // the tables hold modes 0-2 [DESIGN]
    const bool mapped = m != 2 && c >= 0 && c < kSchemeIds && scheme_record_[m][c] != -1;
    if (mapped) {
        ++record_uses_[m][scheme_record_[m][c]];
        return;
    }
    for (int r = 5; r < kRecords; ++r) {
        if (m != 2 && record_uses_[m][r] != 0) continue;
        if (m != 2 && c >= 0 && c < kSchemeIds) {
            scheme_record_[m][c] = r;
            record_uses_[m][r] = 1;
        }
        const int row = ramp_row(c);
        int i = 10;
        while (i < 236 && !(sl.working[i].black() && sl.working[i + 1].black())) ++i;
        if (i < 236) {
            records_[m][r] = {i, i + 9};
            // The original copies from the row it leaves unset for c = 4 and c outside 3-13;
            // the port writes nothing there [DESIGN].
            if (row >= 0)
                for (int k = 0; k < 10; ++k) sl.working[i + k].set(schemes10_[row][k]);
        }
        animate(sl.handle);
        return;
    }
}

// Command 8, seg5:0x3bae: kind 1 at exactly 8 bpp [BYTES ISPACE.DLL seg5:0x3bfe-0x3d65].
void PaletteManager::swap_black(PalHandle pal, int c) {
    const int s = slot_of(pal);
    if (s < 0) return;
    Slot& sl = slots_[s];
    if (sl.kind != 1 || kBitsPixel != 8) return;
    for (int i = 10; i < 245; ++i) {
        PalEntry& e = sl.working[i];
        if (c != 0 && e.r == 1 && e.g == 11 && e.b == 111)
            e.set({});
        else if (c == 0 && e.black())
            e.set({1, 11, 111});
    }
}

// Ordinal 14, seg5:0x50cc [BYTES ISPACE.DLL seg5:0x5160-0x580a].
void PaletteManager::scale(PalHandle pal, int mode, int percent) {
    const int s = slot_of(pal);
    if (s < 0) return;
    Slot& sl = slots_[s];
    const Bases bs = bases(sl.kind);
    if (sl.kind == 0) {
        const int first = mode == 3 ? bs.ramp : bs.top, end = mode == 3 ? bs.top : bs.top + 16;
        for (int i = first; i < end; ++i) {
            std::uint8_t* ch[3] = {&sl.working[i].r, &sl.working[i].g, &sl.working[i].b};
            for (std::uint8_t* p : ch) {
                // imul word [bp+0xa], then cwd (0x99) replaces DX with AX's sign, so the product
                // keeps its low 16 bits as a signed word before idiv by 100; cmp ax, 0xff / jle
                // caps at 255 and keeps a negative, and the low byte is stored [BYTES ISPACE.DLL
                // seg5:0x5186 seg5:0x518c seg5:0x518d seg5:0x518f seg5:0x5192] (shell.md
                // "Palette", Arithmetic; pitfalls.md, byte 0x99).
                int v = w16(static_cast<long>(*p) * percent) / 100;
                if (v > 0xff) v = 0xff;
                *p = static_cast<std::uint8_t>(v);
            }
        }
    } else {
        if (percent > 40) percent = 40;
        const int m = mode < 0 || mode > 2 ? 2 : mode;
        const int from = sl.blend_from[m], to = sl.blend_to[m];
        // FUN seg3:0x130: (to - from) * percent * 25 / 1000 as a long, truncating.
        const auto mix = [&](int t, int f) {
            const long v = static_cast<long>(w16(t - f)) * (percent * 25) / 1000;
            return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v) + f);
        };
        std::vector<Rgb> c;
        const bool ten = m == 0;
        const int n = ten ? 10 : 5;
        for (int k = 0; k < n; ++k) {
            Rgb a = ten ? (to >= 0 && to < kSchemeRows ? schemes10_[to][k] : Rgb{})
                        : (to >= 0 && to < kSchemeRows ? schemes5_[to][k] : Rgb{});
            Rgb b = ten ? (from >= 0 && from < kSchemeRows ? schemes10_[from][k] : Rgb{})
                        : (from >= 0 && from < kSchemeRows ? schemes5_[from][k] : Rgb{});
            c.push_back({mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)});
        }
        const int steps = m == 0 ? 8 : m == 1 ? 7 : 4;
        int pos = m == 0 ? bs.ramp : m == 1 ? bs.grey : bs.top;
        const int groups = m == 0 ? 9 : 4;
        for (int k = 0; k < groups; ++k) {
            int r = c[k].r, g = c[k].g, b = c[k].b;
            const int dr = (c[k + 1].r - c[k].r) / steps, dg = (c[k + 1].g - c[k].g) / steps,
                      db = (c[k + 1].b - c[k].b) / steps;
            for (int e = 0; e < steps; ++e, ++pos) {
                sl.working[pos].set({static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                                     static_cast<std::uint8_t>(b)});
                r += dr;
                g += dg;
                b += db;
            }
        }
        sl.working[pos].set(c[groups]);
    }
    animate(pal);
    if (sl.kind == 1 && percent == 40) sl.blend_from[mode < 0 || mode > 2 ? 2 : mode] =
        sl.blend_to[mode < 0 || mode > 2 ? 2 : mode];
}

// The brightness of seg5:0x26a2's monotonic test: (R - R/4, G, B/2 + B/8), the root of the
// mean square by three Newton steps from the mean [BYTES ISPACE.DLL seg5:0x2fce seg5:0x30f0 seg6:0x7f52].
int PaletteManager::luminance(Rgb c) {
    const long r = c.r - (c.r >> 2), g = c.g, b = (c.b >> 1) + (c.b >> 3);
    const long target = (b * b + r * r + g * g) / 3;
    const long guess = (std::labs(b) + std::labs(g) + std::labs(r)) / 3;
    return w16(isqrt3(guess, target));
}

// seg5:0x26a2 and the command 3 of seg5:0x1b8e [BYTES ISPACE.DLL seg5:0x1be2 seg5:0x26a2].
void PaletteManager::recolour_graphic(PalHandle pal, int id, std::vector<std::uint32_t>& table,
                                      int d, int a, int b, int last, bool ordinal12) {
    const int s = slot_of(pal);
    const std::size_t n = table.size();
    if (n == 0) return;
    if (table.size() < 16) table.resize(16, 0);  // the pass reads 16 entries [DESIGN]
    auto rgb = [&](int k) { return Rgb::of(table[k]); };
    // The fixed set when entry 15 is not black, except that its red byte counts only with
    // DGROUP:0x188: red (+0x66 of the BITMAPINFO) is tested only when DGROUP:0x188 != 0, green
    // (+0x65) and blue (+0x64) always [BYTES ISPACE.DLL seg5:0x2706 seg5:0x270b seg5:0x2710
    // seg5:0x271a seg5:0x2721 seg5:0x272b] (shell.md "Palette").
    {
        const Rgb e15 = rgb(15);
        if ((ordinal12 && e15.r != 0) || e15.g != 0 || e15.b != 0)
            for (int k = 0; k < 16; ++k) table[k] = kFixed16[k];
    }
    // DGROUP:0x15a has no known writer that the intro reaches; the port takes it as 0 [OPEN].
    if (last != 0) {
        for (int k = 0; k < 10; ++k) schemes10_[12][k] = rgb(k);
        for (int k = 10; k < 15; ++k) schemes5_[12][k - 10] = rgb(k);
        if (s >= 0) {
            slots_[s].blend_from[0] = a;
            slots_[s].blend_from[1] = b;
        }
    }
    const auto sa = static_cast<std::int8_t>(a), sb = static_cast<std::int8_t>(b);
    if (id != 0x118) {  // [BYTES seg5:0x28c4]
        if (sa != 12 && sa != -1 && sa >= 0 && sa < kSchemeRows)
            for (int k = 0; k < 10; ++k) table[k] = schemes10_[sa][k].rgb();
        if (sb != 12 && sb != -1 && sb >= 0 && sb < kSchemeRows)
            for (int k = 10; k < 15; ++k) table[k] = schemes5_[sb][k - 10].rgb();
    }
    // Entries 10-14 that repeat one of 0-9 move by R +- 4 and G +- 4 [BYTES seg5:0x2977].
    for (int k = 10; k < 15; ++k)
        for (int j = 0; j < 10; ++j) {
            Rgb e = rgb(k);
            if (rgb(j) != e) continue;
            e.r = static_cast<std::uint8_t>(e.r < 0xfb ? e.r + 4 : e.r - 4);
            e.g = static_cast<std::uint8_t>(e.g < 0xfb ? e.g + 4 : e.g - 4);
            table[k] = e.rgb();
        }
    // DGROUP:0x24 (a +2 nudge) and DGROUP:0x15c (a colour-matching pass) are 0 on the intro's
    // loads [OPEN]; the second, 16-entry palette DGROUP:0x1eae of loads 0 and 1 is not drawn
    // with at 8 bpp and is not built [DESIGN].
    if (d == 0 && s >= 0) {
        Slot& sl = slots_[s];
        const Bases bs = bases(sl.kind);
        if (sa == 4 && sb == -1) {  // [BYTES seg5:0x2cc2]
            sl.blend_from[2] = 4;
            int pos = bs.top;
            Rgb c = rgb(1);
            for (int k = 1; k < 15; ++k, ++pos) {
                c = rgb(k);
                sl.working[pos].set(c);
                c = rgb(k + 1);
            }
            sl.working[pos].set(c);
        } else {
            // DGROUP:0x1ea4 is 0 here: ordinal 12 clears it [BYTES ISPACE.DLL seg6:0xc4].
            const int step1 = sl.kind == 0 ? 3 : 8, step2 = sl.kind == 0 ? 4 : 7;
            bool ramp1 = true, ramp2 = true;
            int prev = 1000;
            for (int k = 0; k < 10; ++k) {
                const int l = luminance(rgb(k));
                if (prev < l) {
                    ramp1 = false;
                    break;
                }
                prev = l;
            }
            prev = 1000;
            for (int k = 10; k < 15; ++k) {
                const int l = luminance(rgb(k));
                if (prev < l) {
                    ramp2 = false;
                    break;
                }
                prev = l;
            }
            if (sa != 12) ramp1 = true;
            if (sb != 12) ramp2 = true;
            const auto write = [&](int from, int to_last, int step, int pos) {
                for (int k = from; k < to_last; ++k) {
                    const Rgb c0 = rgb(k), c1 = rgb(k + 1);
                    const int dr = (c1.r - c0.r) / step, dg = (c1.g - c0.g) / step,
                              db = (c1.b - c0.b) / step;
                    int r = c0.r, g = c0.g, bl = c0.b;
                    for (int e = 0; e < step; ++e, ++pos) {
                        sl.working[pos].set({static_cast<std::uint8_t>(r),
                                             static_cast<std::uint8_t>(g),
                                             static_cast<std::uint8_t>(bl)});
                        r += dr;
                        g += dg;
                        bl += db;
                    }
                }
                sl.working[pos].set(rgb(to_last));
            };
            if (ramp1) write(0, 9, step1, bs.ramp);   // seg5:0x2e87
            if (ramp2) write(10, 14, step2, bs.grey);  // seg5:0x3200
        }
    } else if (d == 2) {  // command 6 for every colour [BYTES seg5:0x34a3]
        for (std::size_t k = 0; k < n; ++k) {
            const Rgb c = rgb(static_cast<int>(k));
            allocate_colour(pal, c.r, c.g, c.b);
        }
    }
    table.resize(std::max<std::size_t>(n, 1));
    if (d == 0 || d == 2) animate(pal);  // seg5:0x1b8e [BYTES seg5:0x1bff]
}

}  // namespace is::gfx
