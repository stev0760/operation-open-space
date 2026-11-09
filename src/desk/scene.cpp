#include "desk/scene.h"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace is::desk {

namespace {

using i32 = std::int32_t;

// ---- the original's arithmetic

// The low word of a value, sign-extended (a 16-bit register).
int lo16(i32 v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(v) & 0xffffu)); }
// ISPACE.DLL seg3:0xfe: the long multiply, low 32 bits.
i32 mul(i32 a, i32 b) { return static_cast<i32>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
// ISPACE.DLL seg3:0x130: the signed long divide, truncating toward zero. The original traps on a
// zero divisor; the port returns 0 there [DESIGN] (no scene reaches it).
i32 ldiv(i32 a, i32 b) {
    if (b == 0) return 0;
    if (a == INT32_MIN && b == -1) return a;
    return a / b;
}
// `cwd; idiv r16` on a word: the sign-extended low word of n over the low word of d, the low word
// of the quotient. The original traps on d = 0 and on -32768 / -1 [DESIGN: 0 and n].
int idiv16(i32 n, i32 d) {
    const int a = lo16(n), b = lo16(d);
    if (b == 0) return 0;
    if (a == -32768 && b == -1) return a;
    return lo16(a / b);
}
// `imul r16` keeping the low word.
int mul16(i32 a, i32 b) { return lo16(mul(lo16(a), lo16(b))); }
// `sar r16, n` and the long `sar` (ISPACE.DLL seg3:0x2b0, or `sar dx; rcr ax`).
int sar16(i32 v, int n) {
    const int x = lo16(v);
    return x >= 0 ? (x >> n) : ~(~x >> n);
}
i32 sar32(i32 v, int n) { return v >= 0 ? (v >> n) : static_cast<i32>(~(~v >> n)); }
// `cwd; sub ax, dx; sar ax, 1`: the word halved toward zero.
int half16(i32 v) { return idiv16(v, 2); }
int abs16(i32 v) { const int x = lo16(v); return lo16(x < 0 ? -x : x); }  // ISPACE.DLL seg2:0x802
i32 labs32(i32 v) { return v < 0 ? static_cast<i32>(0u - static_cast<std::uint32_t>(v)) : v; }  // seg2:0x824

int word_at(const std::uint8_t* p, std::size_t o) {
    return static_cast<std::int16_t>(p[o] | (p[o + 1] << 8));
}
std::int32_t dword_at(const std::uint8_t* p, std::size_t o) {
    return static_cast<std::int32_t>(p[o] | (p[o + 1] << 8) | (p[o + 2] << 16) |
                                     (static_cast<std::uint32_t>(p[o + 3]) << 24));
}
void put_dword(std::uint8_t* p, std::size_t o, std::int32_t v) {
    const auto u = static_cast<std::uint32_t>(v);
    for (int k = 0; k < 4; ++k) p[o + k] = static_cast<std::uint8_t>(u >> (8 * k));
}
std::uint8_t lo8(i32 v) { return static_cast<std::uint8_t>(static_cast<std::uint32_t>(v) & 0xffu); }

// The record word at +5 + 2·code that the solver reads for codes 1-4 (and a line's end-point
// codes above 5): +7 SX, +9 SY, +0xb SZ, +0xd the X position code, and on through +0x17 from the
// solved fields; past +0x19 the raw bytes, which the solver never changes [BYTES ISPACE.DLL
// seg8:0xa93 seg8:0xc41 seg8:0xd64].
int field_word(const Record& r, int offset) {
    switch (offset) {
    case 0x01: return r.pos[0];
    case 0x03: return r.pos[1];
    case 0x05: return r.pos[2];
    case 0x07: return r.size[0];
    case 0x09: return r.size[1];
    case 0x0b: return r.size[2];
    case 0x0d: return r.pos_code[0];
    case 0x0f: return r.pos_code[1];
    case 0x11: return r.pos_code[2];
    case 0x13: return r.size_code[0];
    case 0x15: return r.size_code[1];
    case 0x17: return r.size_code[2];
    case 0x19: return r.string_id;
    default:
        if (offset >= 0 && offset + 1 < static_cast<int>(r.raw.size())) return word_at(r.raw.data(), static_cast<std::size_t>(offset));
        return 0;  // past the record: 0 (ADR 0016 decision 2) [DESIGN]; no scene uses such a code
    }
}

}  // namespace

namespace {
int typed_channel(const Rgb& c, int ch) { return ch == 0 ? c.r : ch == 1 ? c.g : c.b; }
int raw_word(const Header& h, int o) {
    return o >= 0 && o + 1 < static_cast<int>(h.raw.size()) ? word_at(h.raw.data(), static_cast<std::size_t>(o)) : 0;
}
}  // namespace

// Words +6i + 2ch, +0x2c + 6i + 2ch and +0x26 + 2i [BYTES ISPACE.DLL seg8:0x1465-0x1475
// seg8:0x14d0-0x14db seg8:0x1537-0x1548].
int colour_word(const Header& h, int i, int ch) {
    if (h.has_raw) return raw_word(h, 6 * i + 2 * ch);
    return i >= 0 && i < 6 && ch >= 0 && ch < 3 ? typed_channel(h.colour[static_cast<std::size_t>(i)], ch) : 0;
}
int dark_word(const Header& h, int i, int ch) {
    if (h.has_raw) return raw_word(h, 0x2c + 6 * i + 2 * ch);
    return i >= 0 && i < 6 && ch >= 0 && ch < 3 ? typed_channel(h.dark[static_cast<std::size_t>(i)], ch) : 0;
}
int divisor_word(const Header& h, int i) {
    if (h.has_raw) return raw_word(h, 0x26 + 2 * i);
    if (i >= 0 && i < 3) return h.divisor[static_cast<std::size_t>(i)];
    return i >= 3 && i < 6 ? typed_channel(h.dark[0], i - 3) : 0;
}

Scene decode(const fmt::Scene6600& raw) {
    Scene s;
    const std::uint8_t* h = raw.header.data();
    Header& hd = s.header;
    hd.raw = raw.header;
    hd.has_raw = true;
    for (int i = 0; i < 6; ++i) {
        hd.colour[i] = {lo8(word_at(h, 6 * i)), lo8(word_at(h, 6 * i + 2)), lo8(word_at(h, 6 * i + 4))};
        hd.dark[i] = {lo8(word_at(h, 0x2c + 6 * i)), lo8(word_at(h, 0x2e + 6 * i)), lo8(word_at(h, 0x30 + 6 * i))};
    }
    for (int i = 0; i < 3; ++i) hd.divisor[i] = word_at(h, 0x26 + 2 * i);
    hd.count = word_at(h, 0x24);
    hd.mode_x = h[0x50];
    hd.mode_y = h[0x51];
    hd.d5c = dword_at(h, 0x5c);
    hd.d60 = dword_at(h, 0x60);
    hd.d64 = dword_at(h, 0x64);
    hd.d68 = dword_at(h, 0x68);
    for (const auto& b : raw.records) {
        const std::uint8_t* p = b.data();
        Record r;
        r.raw = b;
        r.resolved = p[0] != 0;
        for (int k = 0; k < 3; ++k) {
            r.pos[k] = word_at(p, 1 + 2 * k);
            r.size[k] = word_at(p, 7 + 2 * k);
            r.pos_code[k] = word_at(p, 0xd + 2 * k);
            r.size_code[k] = word_at(p, 0x13 + 2 * k);
        }
        r.string_id = word_at(p, 0x19);
        r.parent = p[0x1b];
        r.parent2 = p[0x1c];
        r.colour = static_cast<std::int8_t>(p[0x1d]);
        r.type = p[0x1e];
        r.slot = static_cast<std::int8_t>(p[0x1f]);
        r.brightness = p[0x20];
        r.f21 = static_cast<std::int8_t>(p[0x21]);
        r.f22 = static_cast<std::int8_t>(p[0x22]);
        r.f23 = static_cast<std::int8_t>(p[0x23]);
        r.f24 = static_cast<std::int8_t>(p[0x24]);
        r.f25 = static_cast<std::int8_t>(p[0x25]);
        s.records.push_back(r);
    }
    return s;
}

// [BYTES ISPACE.DLL seg8:0x2920-0x296b]
std::int32_t Projection::scale(std::int32_t y) const {
    const i32 d3c = d28;  // DGROUP:0xd3c = sext(d28) [BYTES ISPACE.DLL seg8:0x23e-0x245]
    const i32 q = ldiv(mul(y, d3c), 1000);
    return ldiv(mul(d4c, d3c), q + d4c);
}
// [BYTES ISPACE.DLL seg8:0x2971-0x2994]
int Projection::yb(std::int32_t s) const { return lo16(lo16(ldiv(mul(s, d48), 2000)) + d2e); }
// [BYTES ISPACE.DLL seg8:0x2a3a-0x2a6f]: (c6c - Yb) is a word, its 16 x 16 `imul` by 1000 kept
// whole; (W - d28)/2 a word halved toward zero.
int Projection::centre(int y) const {
    return lo16(lo16(ldiv(lo16(c6c - y) * 1000, d44)) + half16(W - d28));
}

// seg8:0x0 [BYTES ISPACE.DLL seg8:0xf-0x24 seg8:0xd0-0x162 seg8:0x162-0x188 seg8:0x18c-0x1af
// seg8:0x1f3-0x211 seg8:0x254-0x2a9]. D38, c6a and d2a (seg8:0x1b2-0x237) have no reader in seg8.
Projection projection(const Header& h, int W, int H, int cx, int cy, int prev_d2c, int prev_d2e) {
    Projection p;
    p.W = W;
    p.H = H;
    p.cx = cx;
    p.cy = cy;
    p.d28 = idiv16(lo16(H * 4), 3);  // `shl ax, 2` on the word
    auto mode = [&](int m, std::int32_t v, int prev) -> int {
        if (m == 6) return lo16(ldiv(mul(v, lo16(W)), 1000));
        if (m == 7) return lo16(ldiv(mul(v, lo16(H)), 1000));
        if (m == 8) return lo16(v);
        return prev;  // another mode stores nothing [BYTES ISPACE.DLL seg8:0x10e seg8:0x159]
    };
    p.d2c = mode(h.mode_x, h.d5c, prev_d2c);
    p.d2e = mode(h.mode_y, h.d60, prev_d2e);
    p.d4c = ldiv(mul(h.d64, p.d28), 1000);
    p.c6c = lo16(ldiv(mul(h.d68, lo16(H)), 1000));
    p.d48 = ldiv(lo16(p.c6c - p.d2e) * 2000, p.d28);
    i32 lc = lo16(half16(p.d28 - W) + p.d2c);
    if (lc == 0) lc = 1;
    p.d44 = ldiv(lo16(p.c6c - p.d2e) * 1000, lc);
    if (p.d44 == 0) p.d44 = 1;
    return p;
}

// seg8:0x1641 [BYTES ISPACE.DLL seg8:0x1641-0x16a2 seg8:0x1791-0x17b9].
bool condition_holds(const Header& h, int v, const Conditions& c) {
    const int at = 0x48 - v;
    if (at < 0 || at >= static_cast<int>(h.raw.size())) return false;
    switch (h.raw[static_cast<std::size_t>(at)]) {
    case 1: return (c.a8 & 0xff) == 1;
    case 2: return c.c70 > 3;
    case 3: return c.async_wave;
    case 4: return c.c70 < 4;
    case 5: return c.stereo;
    default: return false;
    }
}

std::string cpu_text(int c70) { return std::to_string(lo16(100 * c70 + 86)); }

namespace {
std::string clip(std::string s, std::size_t n) {
    if (s.size() > n) s.resize(n);
    const std::size_t z = s.find('\0');
    if (z != std::string::npos) s.resize(z);
    return s;
}
// The cut loop: for i while lstrlen > i, a NUL at the cut ends the string and the loop.
template <typename F>
std::string cut(std::string s, F at) {
    for (std::size_t i = 0; i < s.size(); ++i)
        if (at(i, s[i])) {
            s.resize(i);
            break;
        }
    return s;
}
}  // namespace

std::string cut_video_text(std::string s) {
    return cut(clip(std::move(s), 0x59), [](std::size_t i, char c) { return (i > 16 && c == ' ') || c == '('; });
}
std::string cut_sound_text(std::string s) {
    s = clip(std::move(s), 0x59);
    if (s.size() >= 4 && s.compare(0, 4, "Crea") == 0) s = "Sound Blaster";  // ISPACE.DLL DGROUP:0xac
    return cut(s, [](std::size_t i, char c) { return (i > 16 && c == ' ') || c == '('; });
}
std::string cut_net_key(std::string s) {
    return cut(clip(std::move(s), 0x27), [](std::size_t, char c) { return c == ','; });
}
std::string cut_net_text(std::string s) {
    s = clip(std::move(s), 0x59);
    if (!s.empty() && s[0] == '"') s.erase(0, 1);
    return cut(s, [](std::size_t i, char c) { return (i > 16 && c == ' ') || c == '(' || c == ',' || c == '"'; });
}

// seg8:0x5c86 [BYTES ISPACE.DLL seg8:0x5c95-0x5fa5]: GetSystemDirectory and GetWindowsDirectory
// into buffers of 0x6e, each INI path the Windows directory plus ISPACE.DLL DGROUP:0x45
// "\system.ini", 0x87 "\control.ini" and 0xba "\protocol.ini", NETWORK.INF the system
// directory plus DGROUP:0xdf "\network.inf".
Conditions devices_from(const DeviceSource& src) {
    Conditions c;
    auto read = [&](const char* section, const std::string& key, const char* def, int size, const std::string& file) {
        return src.profile ? src.profile(section, key, def, size, file) : std::string(def);
    };
    const std::string system_ini = src.windows_dir + "\\system.ini";
    // [Boot.description] display.drv, default "Video Card", into DGROUP:0x1b34 (0x5a bytes)
    // [BYTES ISPACE.DLL seg8:0x5cae-0x5cd5] [BYTES ISPACE.DLL DGROUP:0x51 DGROUP:0x5c DGROUP:0x68].
    c.video = cut_video_text(read("Boot.description", "display.drv", "Video Card", 0x5a, system_ini));
    // [drivers] Wave, default "" (0x28 bytes), then CONTROL.INI [drivers.desc] under that value,
    // default "Sound Card", into DGROUP:0x1b8e [BYTES ISPACE.DLL seg8:0x5d16-0x5d6b]
    // [BYTES ISPACE.DLL DGROUP:0x79 DGROUP:0x7a DGROUP:0x7f DGROUP:0x87 DGROUP:0x94 DGROUP:0x9f].
    const std::string wave = clip(read("drivers", "Wave", "", 0x28, system_ini), 0x27);
    c.sound = cut_sound_text(read("drivers.desc", wave, "Sound Card", 0x5a, src.windows_dir + "\\control.ini"));
    // DGROUP:0x22aa = 1, 0x22ab = 0; no device clears 0x22aa; device 0's WAVECAPS_SYNC (0x10)
    // clears it and more than one channel sets 0x22ab [BYTES ISPACE.DLL seg8:0x5dd5-0x5e11].
    c.async_wave = true;
    c.stereo = false;
    if (src.wave_devs == 0) {
        c.async_wave = false;
    } else {
        if ((src.wave_support & 0x10u) != 0) c.async_wave = false;
        if (src.wave_channels > 1) c.stereo = true;
    }
    // DGROUP:0x22a9 = 1; PROTOCOL.INI [network.setup] netcard, default "" (0x28 bytes), cut at
    // ','; NETWORK.INF [netcard] under it, default "", into DGROUP:0x1be8; either empty clears
    // DGROUP:0x22a9 [BYTES ISPACE.DLL seg8:0x5e16-0x5eb2] [BYTES ISPACE.DLL DGROUP:0xba DGROUP:0xc8
    // DGROUP:0xc9 DGROUP:0xd1 DGROUP:0xdf DGROUP:0xec DGROUP:0xed].
    c.net_found = true;
    const std::string key = clip(read("network.setup", "netcard", "", 0x28, src.windows_dir + "\\protocol.ini"), 0x27);
    if (key.empty()) c.net_found = false;
    const std::string net = clip(read("netcard", cut_net_key(key), "", 0x5a, src.system_dir + "\\network.inf"), 0x59);
    if (net.empty()) c.net_found = false;
    c.net = cut_net_text(net);
    // DGROUP:0x22a8 = 0xff; WF_CPU286 (2) -> 2; else WF_CPU386 (4) -> 3, with WF_80x87 (0x400)
    // DGROUP:0x22a8 = 1; else WF_CPU486 (8) -> 4 and DGROUP:0x22a8 = 0; else 0. DGROUP:0x1c70 is
    // the class, DGROUP:0xd1c its itoa(100*class + 86) (cpu_text) [BYTES ISPACE.DLL seg8:0x5f47-0x5fa5].
    c.a8 = 0xff;
    const std::uint32_t f = src.win_flags;
    if (f & 0x2u) {
        c.c70 = 2;
    } else if (f & 0x4u) {
        c.c70 = 3;
        if (f & 0x400u) c.a8 = 1;
    } else if (f & 0x8u) {
        c.c70 = 4;
        c.a8 = 0;
    } else {
        c.c70 = 0;
    }
    return c;
}

// seg8:0xa36's resolve passes [BYTES ISPACE.DLL seg8:0xa57 seg8:0xa60 seg8:0xa76 seg8:0xa93
// seg8:0xb00 seg8:0xb5d seg8:0xc41 seg8:0xc5d seg8:0xd64 seg8:0xdaa seg8:0xe00 seg8:0xe40
// seg8:0xfd9 seg8:0x1036 seg8:0x120a seg8:0x12ef seg8:0x13d0 seg8:0x13dc].
int solve(Scene& s, const Projection& p) {
    auto& R = s.records;
    const int n = static_cast<int>(R.size());
    int max_slot = -1;
    if (n == 0) return max_slot;
    for (bool changed = true; changed;) {
        changed = false;
        for (int i = 1; i < n; ++i) {
            Record& r = R[static_cast<std::size_t>(i)];
            if (r.resolved || r.parent >= n || !R[static_cast<std::size_t>(r.parent)].resolved) continue;
            const int t = r.type;
            if (t < 1 || t > 5) continue;
            if (t == 2 && (r.parent2 >= n || !R[static_cast<std::size_t>(r.parent2)].resolved)) continue;
            const Record& pa = R[static_cast<std::size_t>(r.parent)];
            for (int k = 0; k < 3; ++k) {
                // phase 1: the position
                int c = r.pos_code[k];
                bool has = true;
                i32 ref = 0;
                if (c == 0) ref = pa.size[k];
                else if (c == 5) ref = R[0].size[0];
                else if (c >= 1 && c < 5) ref = field_word(pa, 5 + 2 * c);
                else has = false;
                if (has && ref != -1) {
                    r.pos[k] = lo16(ldiv(mul(r.pos[k], ref), 1000) + pa.pos[k]);
                    if (k == 0) r.pos[0] = lo16(r.pos[0] - ldiv(pa.size[0], 2));
                    if (k == 2) r.pos[2] = lo16(r.pos[2] + pa.size[2]);
                }
                c = r.size_code[k];
                if (t != 2) {
                    has = true;
                    if (c == 0) ref = pa.size[0];
                    else if (k != 0 && c == 4) ref = r.size[0];
                    else if (c == 5) ref = R[0].size[0];
                    else if (c >= 1 && c < 6) ref = field_word(pa, 5 + 2 * c);
                    else has = false;
                    if (has && ref != -1) r.size[k] = lo16(ldiv(mul(r.size[k], ref), 1000));
                } else {
                    const Record& q = R[static_cast<std::size_t>(r.parent2)];
                    if (c == 0) ref = q.size[k];
                    else if (c == 5) ref = R[0].size[0];
                    else ref = field_word(q, 5 + 2 * c);
                    r.size[k] = lo16(ldiv(mul(r.size[k], ref), 1000) + q.pos[k]);
                    if (k == 0) r.size[0] = lo16(r.size[0] - ldiv(q.size[0], 2));
                    if (k == 2) r.size[2] = lo16(r.size[2] + q.size[2]);
                }
            }
            // phase 2: screen-unit codes
            const i32 sc = p.scale(r.pos[1]);
            const int yb = p.yb(sc);
            const i32 half = half16(p.W - p.d28);
            const i32 off = ldiv(lo16(p.c6c - yb) * 1000, p.d44);
            auto per_mille = [&](i32 v, int c) -> i32 {
                switch (c) {
                case 6: return ldiv(v * p.W, 1000);
                case 7: return ldiv(v * p.H, 1000);
                case 8: return v;
                case 9: return ldiv(v * p.cx, 1000);
                case 10: return ldiv(v * p.cy, 1000);
                default: return std::max<i32>(ldiv(v * p.H, 1000), c);
                }
            };
            if (r.pos_code[0] >= 6 && sc != 0) {
                const i32 x = per_mille(r.pos[0], r.pos_code[0]);
                r.pos[0] = lo16(ldiv((x - off - half) * 1000, sc));
            }
            if (r.pos_code[2] >= 6 && sc != 0) {
                const i32 y = per_mille(r.pos[2], r.pos_code[2]);
                r.pos[2] = lo16(ldiv((yb - y) * 1000, sc));
            }
            for (int k : {0, 2}) {
                const int c = r.size_code[k];
                if (c < 6 || sc == 0) continue;
                const i32 v = r.size[k];
                i32 m = 0;
                switch (c) {
                case 6: m = v * p.W; break;
                case 7: m = v * p.H; break;
                case 8: m = v * 1000; break;
                case 9: m = v * p.cx; break;
                case 10: m = v * p.cy; break;
                default: m = std::max<i32>(v * p.H, static_cast<i32>(c) * 1000); break;
                }
                r.size[k] = lo16(ldiv(m, sc));
            }
            r.resolved = true;
            max_slot = std::max(max_slot, r.slot);
            changed = true;
        }
    }
    return max_slot;
}

// ---- colours

int peak_of(const Header& h, int i) {
    // DGROUP:0x1c42 is written for i < +0x24 only; above, its BSS start 0 (ADR 0016 decision 1)
    // [DESIGN]; no record of the five scenes names such a colour.
    if (i < 0 || i >= h.count) return 0;
    int k = 0, best = -1;
    for (int c = 0; c < 3; ++c) {
        const int v = colour_word(h, i, c);
        if (v > best) best = v, k = c;  // `cmp; jle skip` [BYTES ISPACE.DLL seg8:0x1501-0x1504]
    }
    return k;
}

int level_of(const Header& h, const Record& r) {
    const int bright = static_cast<std::int8_t>(static_cast<std::uint8_t>(r.brightness));  // CBW
    return idiv16(mul16(colour_word(h, r.colour, peak_of(h, r.colour)), bright), 50);
}

std::uint32_t shade_colour(const Header& h, int col, int peak, int level) {
    if (level > 0xff) level = 0xff;  // `cmp ax, 0xff; jle` [BYTES ISPACE.DLL seg8:0x2802-0x2807]
    std::uint8_t ch[3];
    const i32 div = colour_word(h, col, peak);
    for (int j = 0; j < 3; ++j)
        ch[j] = j == peak ? lo8(level) : lo8(ldiv(mul(colour_word(h, col, j), level), div));
    return 0x02000000u | (static_cast<std::uint32_t>(ch[2]) << 16) | (static_cast<std::uint32_t>(ch[1]) << 8) | ch[0];
}

Rgb shade_rgb(const Rgb& colour, int level) {
    Header h;
    h.count = 1;
    h.has_raw = true;
    const int words[3] = {colour.r, colour.g, colour.b};
    for (int j = 0; j < 3; ++j) {
        h.raw[static_cast<std::size_t>(2 * j)] = static_cast<std::uint8_t>(words[j]);
        h.raw[static_cast<std::size_t>(2 * j + 1)] = 0;
    }
    const std::uint32_t c = shade_colour(h, 0, peak_of(h, 0), level);
    return {lo8(static_cast<i32>(c)), lo8(static_cast<i32>(c >> 8)), lo8(static_cast<i32>(c >> 16))};
}

std::vector<std::vector<Rgb>> shade_ramps(const Header& h) {
    std::vector<std::vector<Rgb>> out;
    for (int i = 0; i < std::min(h.count, 6); ++i) {
        const int k = peak_of(h, i);
        const int n = idiv16(colour_word(h, i, k) - dark_word(h, i, k), divisor_word(h, i));
        std::vector<Rgb> ramp;
        for (int j = 0; j < n; ++j) {
            auto mix = [&](int c) {
                return lo8(idiv16(mul16(colour_word(h, i, c) - dark_word(h, i, c), j), n) + dark_word(h, i, c));
            };
            ramp.push_back({mix(0), mix(1), mix(2)});
        }
        out.push_back(std::move(ramp));
    }
    return out;
}

// ---- ordinal 34

std::vector<Op> frame_ops(const Op& f) {
    std::vector<Op> v;
    auto op = [&](Op::Kind k) -> Op& {
        v.push_back(Op{});
        v.back().kind = k;
        v.back().record = f.record;
        return v.back();
    };
    const int x = f.x, y = f.y, w = f.w, h = f.h;
    int light = 0xff, dark = 0;  // modes 1 and 2
    if (f.mode == 0) light = -1, dark = 0xc0;
    if (f.mode == 3) light = 0x80, dark = 0xff;
    op(Op::kStock).stock = f.mode == 0 ? 4 : 1;  // BLACK_BRUSH, LTGRAY_BRUSH [BYTES ISPACE.DLL seg8:0x5bb-0x611]
    Op& pb = op(Op::kPatBlt);
    pb.x = x, pb.y = y, pb.w = w, pb.h = h, pb.rop = kPatCopy;
    auto pen = [&](int g, std::uint16_t site) {
        Op& p = op(Op::kPen);
        p.width = 1;
        p.colour = static_cast<std::uint32_t>(g) * 0x010101u;
        p.site = site;
    };
    auto pt = [&](Op::Kind k, int px, int py) {
        Op& p = op(k);
        p.x = px, p.y = py;
    };
    pen(dark, 0x65c);  // [BYTES ISPACE.DLL seg8:0x643-0x6a3]
    pt(Op::kMoveTo, x + w, y);
    pt(Op::kLineTo, x + w, y + h);
    pt(Op::kLineTo, x, y + h);
    if (light != -1) {  // [BYTES ISPACE.DLL seg8:0x6a8-0x702]
        pen(light, 0x6c7);
        pt(Op::kMoveTo, x + w, y);
        pt(Op::kLineTo, x, y);
        pt(Op::kLineTo, x, y + h);
    }
    if (f.mode == 2) {  // [BYTES ISPACE.DLL seg8:0x707-0x758]
        pen(0x80, 0x71f);
        pt(Op::kMoveTo, x, y + h);
        pt(Op::kLineTo, x + w, y + h);
    }
    return v;
}

// ---- the strip filler ISPACE.DLL seg8:0x1fe6

void strip_fill(std::vector<Op>& out, const Header& h, const Projection& p, const std::array<Point, 4>& P,
                int col, int peak, int level, int A, int B, bool rows, int record) {
    auto brush = [&](int yi, std::uint16_t site) {
        // sar16(lo16((L2*(Yi - d2e)^2)/(c6c - d2e)^2), 1), the squares and the product the low
        // 32 bits [BYTES ISPACE.DLL seg8:0x2109-0x214b seg8:0x26a9-0x26e4].
        const i32 L2 = lo16(2 * level);
        const i32 dy = lo16(yi - p.d2e), dc = lo16(p.c6c - p.d2e);
        const int lv = sar16(lo16(ldiv(mul(L2, mul(dy, dy)), mul(dc, dc))), 1);
        Op o;
        o.kind = Op::kBrush;
        o.colour = shade_colour(h, col, peak, lv);
        o.record = record;
        o.site = site;
        out.push_back(o);
    };
    auto blt = [&](int x, int y, int w, int hh) {
        Op o;
        o.kind = Op::kPatBlt;
        o.x = lo16(x), o.y = lo16(y), o.w = lo16(w), o.h = lo16(hh), o.rop = kPatCopy;
        o.record = record;
        out.push_back(o);
    };
    const int dxa = lo16(P[2].x - P[3].x);        // [bp-0x20] [BYTES ISPACE.DLL seg8:0x1ffa-0x2005]
    int e = lo16(P[3].y - P[0].y);                // [bp-0x1e] [BYTES ISPACE.DLL seg8:0x2008-0x2017]
    if (e == 0) e = -1;
    const int dir = e < 0 ? -1 : 1;               // [bp-0x12] [BYTES ISPACE.DLL seg8:0x2028-0x2035]
    if (rows) {
        // seg8:0x2043-0x250b [BYTES ISPACE.DLL seg8:0x2043-0x209d seg8:0x20a8-0x2370 seg8:0x236f-0x250a]
        const i32 dx0 = lo16(P[0].x - P[3].x), dx1 = lo16(P[1].x - P[2].x);
        const int E = abs16(e);
        int t = 5;
        if (E < 45) t = 4;
        if (E < 35) t = 3;
        if (E < 20) t = 2;
        int pxl = 0, pxr = 0, py = 0;
        for (i32 a = 0; a <= lo16(E + t); a += t) {
            const i32 c = std::min<i32>(a, E);
            const int yi = dir == 1 ? lo16(A - ldiv(mul(lo16(A - B), c), e)) : lo16(ldiv(mul(lo16(A - B), c), e) + A);
            brush(yi, 0x2159);
            int xl, y, w;
            if (dir == 1) {
                xl = lo16(ldiv(mul(c, dx0), e) + P[3].x);
                y = lo16(P[0].y - lo16(c) + e);
                w = lo16(ldiv(mul(c, dx1 - dx0), e) + dxa + 1);
            } else {
                xl = lo16(P[3].x - ldiv(mul(c, dx0), e));
                y = lo16(P[0].y + lo16(c) + e);
                w = lo16(ldiv(mul(c, dx0 - dx1), e) + dxa + 1);
            }
            blt(xl, y, w, t);
            const int xr = lo16(xl + w);
            if (c > 0) {
                for (int k = 1; k < t; ++k) {
                    if (dir == 1) {
                        if (pxl > xl) blt(pxl, py - k + t - 1, idiv16(mul16(xl - pxl, k), t), 1);
                        else if (pxl < xl) blt(xl, y + k, idiv16(mul16(pxl - xl, k), t), 1);
                        if (pxr > xr) blt(xr, y + k, idiv16(mul16(pxr - xr, k), t), 1);
                        else if (pxr < xr) blt(pxr, py - k + t - 1, idiv16(mul16(xr - pxr, k), t), 1);
                    } else {
                        if (pxl > xl) blt(pxl, y - t + k, idiv16(mul16(xl - pxl, k), t), 1);
                        else if (pxl < xl) blt(xl, y + k - 1, idiv16(mul16(pxl - xl, t - k), t), 1);
                        if (pxr > xr) blt(xr, y + k - 1, idiv16(mul16(pxr - xr, t - k), t), 1);
                        else if (pxr < xr) blt(pxr, y - t + k, idiv16(mul16(xr - pxr, k), t), 1);
                    }
                }
            }
            pxl = xl, pxr = xr, py = y;
        }
        return;
    }
    // seg8:0x250d-0x27e3 [BYTES ISPACE.DLL seg8:0x2515-0x2559 seg8:0x264b-0x27e3 seg8:0x255f-0x2617]
    const i32 dy0 = lo16(P[1].y - P[0].y), dy3 = lo16(P[2].y - P[3].y);
    int t = 5;
    if (dxa < 45) t = 4;
    if (dxa < 35) t = 3;
    if (dxa < 20) t = 2;
    int ptop = 0, pbot = 0;
    for (i32 a = 0; a <= lo16(dxa + t); a += t) {
        const i32 c = std::min<i32>(a, dxa);
        const int yi = lo16(A - ldiv(mul(lo16(A - B), c), dxa));
        brush(yi, 0x26fc);
        const int x = lo16(P[3].x + lo16(c));
        const int top = lo16(ldiv(mul(c, dy0), dxa) + P[0].y);
        const int hh = lo16(ldiv(mul(c, dy3 - dy0), dxa) + e + 1);
        blt(x, top, t, hh);
        const int bot = lo16(top + hh);
        if (a > 0) {
            for (int k = 1; k < t; ++k) {
                if (ptop > top) blt(x - t + k, ptop, 1, idiv16(mul16(top - ptop, k), t));
                else if (ptop < top) blt(x + k - 1, top, 1, idiv16(mul16(ptop - top, t - k), t));
                if (pbot > bot) blt(x + k - 1, bot, 1, idiv16(mul16(pbot - bot, t - k), t));
                else if (pbot < bot) blt(x - t + k, pbot, 1, idiv16(mul16(bot - pbot, k), t));
            }
        }
        ptop = top, pbot = bot;
    }
}

namespace {

// One projected face of a box at depth Y (s6-scene.md 1.1, 1.4) [BYTES ISPACE.DLL
// seg8:0x2920-0x2a8a seg8:0x2ab8-0x2c27].
struct Face {
    i32 s = 0;               // [bp-0xe]
    int yb = 0;              // [bp-0x1a] front, [bp-0x1c] back
    i32 xp = 0, zp = 0, szp = 0, sxp = 0;   // X', Z', SZ', SX' (dwords)
    int top = 0, centre = 0, left = 0, right = 0;
};
Face face_at(const Projection& p, i32 X, i32 Y, i32 SX, i32 Z, i32 SZ) {
    Face f;
    f.s = p.scale(Y);
    f.yb = p.yb(f.s);
    f.xp = ldiv(mul(X, f.s), 1000);
    f.zp = ldiv(mul(Z, f.s), 1000);
    f.szp = ldiv(mul(SZ, f.s), 1000);
    f.sxp = ldiv(mul(SX, f.s), 1000);
    f.top = lo16(f.yb - lo16(f.szp) - lo16(f.zp));
    f.centre = lo16(p.centre(f.yb) + lo16(f.xp));
    const int h = lo16(ldiv(f.sxp, 2));
    f.left = lo16(f.centre - h);   // [bp-0x32] / [bp-0x3e]
    f.right = lo16(f.centre + h);  // [bp-0x36] / [bp-0x3a]
    return f;
}

struct Drawer {
    const Scene& s;
    const Projection& p;
    const Inputs& in;
    Render& out;
    int index = 0;
    Rect d30;  // ISPACE.DLL DGROUP:0xd30-0xd36, the box's SetRect

    const Header& hd() const { return s.header; }
    void brush(std::uint32_t colour, std::uint16_t site) {
        Op o;
        o.kind = Op::kBrush;
        o.colour = colour;
        o.record = index;
        o.site = site;
        out.ops.push_back(o);
    }
    void blt(int x, int y, int w, int h, std::uint32_t rop = kPatCopy) {
        Op o;
        o.kind = Op::kPatBlt;
        o.x = lo16(x), o.y = lo16(y), o.w = lo16(w), o.h = lo16(h), o.rop = rop;
        o.record = index;
        out.ops.push_back(o);
    }
    void frame(int x, int y, int w, int h, int mode) {
        Op o;
        o.kind = Op::kFrame;
        o.x = lo16(x), o.y = lo16(y), o.w = lo16(w), o.h = lo16(h), o.mode = mode;
        o.record = index;
        out.ops.push_back(o);
    }
    // The shade of a depth with the pins' and cross's colour index 0, channel 0.
    std::uint32_t c0(int level) const { return shade_colour(hd(), 0, 0, level); }

    // ISPACE.DLL seg8:0x5978 (s6-scene.md 1.9) [BYTES ISPACE.DLL seg8:0x5986-0x5b30].
    void text(int x, int y, int size, const std::string& str, int mode, int yb, int r, int g, int b) {
        Op o;
        o.kind = Op::kText;
        o.record = index;
        const int v[3] = {r, g, b};
        int c[3];
        for (int k = 0; k < 3; ++k) {
            c[k] = lo16(ldiv(mul(lo16(v[k] << 4), lo16(yb - p.d2e)), lo16(p.c6c - p.d2e)));
            if (c[k] != 0) {
                c[k] = lo16(c[k] + 400);
                if (c[k] > 0xff0) c[k] = 0xff0;
            }
            if (in.depth < 8) c[k] = 0xff0;
        }
        o.colour = 0x02000000u | (static_cast<std::uint32_t>(lo8(sar16(c[2], 4))) << 16) |
                   (static_cast<std::uint32_t>(lo8(sar16(c[1], 4))) << 8) | lo8(sar16(c[0], 4));
        if (mode == 1) o.font_h = idiv16(size, 6);
        else if (mode == 2 || mode == 3) o.font_h = half16(size);
        // Other modes leave the locals unwritten: 0 (ADR 0016 decision 1) [DESIGN]; every call
        // passes 1, 2 or 3.
        if (mode >= 1 && mode <= 3) o.font_w = idiv16(size, 3);
        o.weight = mode == 3 ? 700 : 0;
        if (mode == 1 || mode == 3) {
            o.align = 6;  // TA_CENTER | TA_TOP, with y moved up [BYTES ISPACE.DLL seg8:0x5a53-0x5a67]
            y = lo16(y + idiv16(size, -12));
        } else if (mode == 2) {
            o.align = 0;
        }
        o.x = lo16(x);
        o.y = lo16(y);
        o.text = str;
        o.site = 0x5b30;
        out.ops.push_back(o);
    }

    // The box ISPACE.DLL seg8:0x28de (s6-scene.md 1.4) [BYTES ISPACE.DLL seg8:0x28de-0x386b].
    void box(i32 X, i32 Y, i32 SY, i32 SX, i32 Z, i32 SZ, int f21, int f22, int col, int peak, int L,
             const std::string& str, int f24, int f25, int f23) {
        if (L < 0) L = lo16(-mul16(L, 10));               // [BYTES ISPACE.DLL seg8:0x28ee-0x28fa]
        const bool bevel = f21 == 1;                       // [BYTES ISPACE.DLL seg8:0x28fd-0x290f]
        if (bevel) f21 = 0;
        const i32 L2 = lo16(2 * L);                        // [BYTES ISPACE.DLL seg8:0x2914-0x291a]
        const Face F = face_at(p, X, Y, SX, Z, SZ);
        d30 = {F.left, F.top, lo16(F.left + lo16(F.sxp)), lo16(F.top + lo16(F.szp))};  // seg8:0x2aaa
        if (col == -1) return;                             // [BYTES ISPACE.DLL seg8:0x2aaf-0x2ab5]
        const Face Bk = face_at(p, X, Y + SY, SX, Z, SZ);
        std::array<Point, 4> P = {Point{Bk.left, Bk.top}, Point{Bk.right, Bk.top}, Point{F.right, F.top},
                                  Point{F.left, F.top}};
        if (F.top > p.d2e) {                               // the top face [BYTES ISPACE.DLL seg8:0x2c2a-0x2c53]
            strip_fill(out.ops, hd(), p, P, col, peak, L, F.yb, Bk.yb, true, index);
            if (f23 != 0 && f24 == 0 && f23 != 1000) {     // [BYTES ISPACE.DLL seg8:0x2c56-0x2cd4]
                const int size = idiv16(mul16(lo16(Bk.right - F.left - Bk.left + F.right), f23), 200);
                text(half16(F.right + F.left), half16(Bk.top + F.top), size, str, 1, half16(Bk.yb + F.yb), 300, 300, 300);
            }
        } else if (lo16(lo16(F.szp) + F.top) < p.d2e) {   // the bottom face [BYTES ISPACE.DLL seg8:0x2cd9-0x2d2c]
            P[0].y = P[1].y = lo16(Bk.top + lo16(Bk.szp));
            P[2].y = P[3].y = lo16(F.top + lo16(F.szp));
            strip_fill(out.ops, hd(), p, P, col, peak, L, F.yb, Bk.yb, true, index);
        }
        const int fb = lo16(F.top + lo16(F.szp)), bb = lo16(Bk.top + lo16(Bk.szp));
        if (Bk.right > F.right) {                          // the right face [BYTES ISPACE.DLL seg8:0x2d2f-0x2d89]
            const std::array<Point, 4> Q = {Point{F.right, F.top}, Point{Bk.right, Bk.top}, Point{Bk.right, bb},
                                            Point{F.right, fb}};
            strip_fill(out.ops, hd(), p, Q, col, peak, idiv16(mul16(L, 3), 4), F.yb, Bk.yb, false, index);
            if (f22 != 0) pins(X, Y, SY, SX, Z, SZ, f22, true);
        } else if (F.left > Bk.left) {                     // the left face [BYTES ISPACE.DLL seg8:0x306a-0x30c4]
            const std::array<Point, 4> Q = {Point{Bk.left, Bk.top}, Point{F.left, F.top}, Point{F.left, fb},
                                            Point{Bk.left, bb}};
            strip_fill(out.ops, hd(), p, Q, col, peak, idiv16(mul16(L, 5), 4), Bk.yb, F.yb, false, index);
            if (f22 != 0) pins(X, Y, SY, SX, Z, SZ, f22, false);
        }
        // The front face: s and SX' again [BYTES ISPACE.DLL seg8:0x33b3-0x3420], the shade
        // [BYTES ISPACE.DLL seg8:0x3423-0x3461], the 2-px shift [BYTES ISPACE.DLL seg8:0x3464-0x34c2].
        const i32 sf = p.scale(Y);
        i32 sx2 = ldiv(mul(SX, sf), 1000);
        const int shade = sar16(lo16(ldiv(mul(ldiv(mul(L2, 12), 13), lo16(F.yb - p.d2e)), lo16(p.c6c - p.d2e))), 1);
        int left = F.left;
        if (left > Bk.left) {
            left = lo16(left + 2);
            sx2 -= 2;
        }
        const int top = F.top, szlo = lo16(F.szp);
        if (f21 >= 100) {                                  // [BYTES ISPACE.DLL seg8:0x34c5-0x3542]
            Op o;
            o.kind = Op::kPattern;
            o.pattern = f21 == 100 ? Pattern::kMetal : f21 == 120 ? Pattern::kGraphic76 : Pattern::kNone;
            o.record = index;
            o.site = 0x3508;
            out.ops.push_back(o);
            f21 = 0;
            blt(left, top, lo16(sx2), szlo + 1);
        } else {                                           // [BYTES ISPACE.DLL seg8:0x3544-0x358e]
            brush(shade_colour(hd(), col, peak, shade), 0x355c);
            blt(left, top, lo16(sx2), szlo + 1);
        }
        d30 = {left, top, lo16(left + lo16(sx2)), lo16(top + szlo)};  // [BYTES ISPACE.DLL seg8:0x3593-0x35af]
        if (bevel) {                                       // [BYTES ISPACE.DLL seg8:0x35b4-0x35ea]
            const int e8 = lo16(sar32(sx2, 3)), q4 = lo16(lo16(sx2) - lo16(sar32(sx2, 2)));
            blt(left + e8, top + e8, q4, q4, kBlackness);
        }
        if (f23 != 0 && f24 != 0) {                        // [BYTES ISPACE.DLL seg8:0x35ef-0x35fe]
            const int size = idiv16(mul16(lo16(sx2), f23), 100);
            if (f25 == 0) {                                // the label [BYTES ISPACE.DLL seg8:0x3604-0x364a]
                text(left + idiv16(sx2, 20), top + idiv16(szlo, 17), size, str, 2, F.yb, 300, 260, 100);
            } else if (!str.empty() && str[0] == 'H') {    // the plate [BYTES ISPACE.DLL seg8:0x364d-0x373c]
                const int c = lo16(half16(sx2) + left);
                const int y0 = lo16(idiv16(szlo, 3) + top);
                const int up = idiv16(mul16(size, 2), -3);
                const int w13 = idiv16(mul16(size, 13), 3), h5 = half16(mul16(size, 5));
                frame(c - mul16(size, 2), y0 + up, w13, h5 + 5, 2);
                frame(mul16(3 - size, 2) + c, up + y0 + 6, w13 - 12, h5 - 7, 3);
                text(c, y0, size, "HIGH", 3, F.yb, 192, 0, 0);       // ISPACE.DLL DGROUP:0x38
                text(c, y0 + size, size, "VOLTAGE", 3, F.yb, 192, 0, 0);  // DGROUP:0x3d
            }
        }
        if (f21 != 0) {                                    // the vents [BYTES ISPACE.DLL seg8:0x3742-0x3868]
            const int ymid = lo16(lo16(sar32(F.szp, 1)) + top);
            const int w = lo16(ldiv(sx2, lo16(mul16(f21, 5))));
            int x = lo16(w + left);
            const int hv = lo16(sar32(F.szp, 2));
            for (int i = 0; i < f21; ++i) {
                brush(c0(idiv16(mul16(shade, 7), 4)), 0x379f);
                blt(x - w, ymid, w, hv + 1);
                brush(shade_colour(hd(), col, peak, lo16(2 * shade)), 0x3805);
                blt(x - w, ymid + hv + 1, w, hv + 1);
                x = lo16(x + ldiv(sx2 - w, lo16(f21 - 1)));
            }
        }
    }

    // The pins of a side face (s6-scene.md 1.4) [BYTES ISPACE.DLL seg8:0x2d9d-0x3064 (right)
    // seg8:0x30d8-0x33b0 (left)]. [OPEN: the extents are the analyst's reading, unchecked by a
    // verifier (ispace.md ordinal 32); a verify round on s6-scene.md 1.4 settles them] (review M2).
    void pins(i32 X, i32 Y, i32 SY, i32 SX, i32 Z, i32 SZ, int n, bool right) {
        for (int i = 0; i < n; ++i) {
            const i32 yi = ldiv(mul(i, SY), n) + Y;
            const i32 s = p.scale(yi);
            const int yb = p.yb(s);
            const i32 xp = ldiv(mul(X, s), 1000), zp = ldiv(mul(Z, s), 1000), h2 = ldiv(mul(SZ, s), 2000);
            i32 w32 = ldiv(mul(s, lo16(ldiv(ldiv(SY, n), 6))), 1500);
            const int w = w32 < 1 ? 1 : lo16(w32);
            const int centre = lo16(p.centre(yb) + lo16(xp));
            const i32 sxp = ldiv(mul(SX, s), 1000);
            const int mid = lo16(yb - lo16(zp) - lo16(h2));
            const int h = lo16(sar32(h2, 1));
            int v;
            int edge;
            if (right) {
                edge = lo16(lo16(ldiv(sxp, 2)) + centre);
                v = sar16(lo16(ldiv(lo16(yb - p.d2e) * 1800, lo16(p.c6c - p.d2e))) + 0x280, 4);
            } else {
                edge = lo16(centre - lo16(ldiv(sxp, 2)));
                v = sar16(lo16(ldiv(mul(lo16(yb - p.d2e), 1 << 11), lo16(p.c6c - p.d2e))) + 0x300, 4);
            }
            brush(c0(idiv16(lo16(v << 2), 5)), right ? 0x2fc5 : 0x3308);
            blt(right ? edge : edge - w, mid, w, h);
            brush(c0(v), right ? 0x301c : 0x3363);
            blt(right ? lo16(sar16(w, 2) + edge) : lo16(edge - sar16(w, 2) - w), mid + h, w, h);
        }
    }

    // The cylinder ISPACE.DLL seg8:0x3874, type 3 (s6-scene.md 1.7) [BYTES ISPACE.DLL
    // seg8:0x3884-0x41f9].
    void cylinder(i32 X, i32 Y, i32 Z, i32 SX, i32 SY, i32 SZ, int col, int peak, int L, int f21) {
        int t = 2;                                         // the pushed [bp+0x26] [BYTES ISPACE.DLL seg8:0x18b2]
        const i32 L2 = lo16(2 * L);                        // [BYTES ISPACE.DLL seg8:0x3884-0x388d]
        const i32 sf = p.scale(Y);
        const int ybf = p.yb(sf);
        const i32 zpf = ldiv(mul(Z, sf), 1000), szpf = ldiv(mul(SZ, sf), 1000);
        const i32 sm = p.scale(ldiv(SY, 2) + Y);
        const int ybm = p.yb(sm);
        const i32 xpm = ldiv(mul(X, sm), 1000), sxm = ldiv(mul(SX, sm), 1000);
        const int cm = lo16(p.centre(ybm) + lo16(xpm));
        const int hm = lo16(ldiv(sxm, 2));
        const int left_m = lo16(cm - hm), right_m = lo16(cm + hm);
        const i32 zpm = ldiv(mul(Z, sm), 1000), szpm = ldiv(mul(SZ, sm), 1000);
        const int bot_m = lo16(ybm - lo16(zpm));
        const i32 R = half16(right_m - left_m);
        const int bot_f = lo16(ybf - lo16(zpf));
        const int top_m = lo16(bot_m - lo16(szpm));
        const int top_f = lo16(bot_f - lo16(szpf));
        i32 Ry[2] = {lo16(bot_f - bot_m), lo16(top_f - top_m)};
        if (Ry[1] == 0) Ry[1] = 1;                         // [BYTES ISPACE.DLL seg8:0x3b39-0x3b5d]
        if (Ry[0] == 0) Ry[0] = 1;
        auto isq = [](i32 g, i32 Q) {                      // ISPACE.DLL seg6:0x7f52 [BYTES ISPACE.DLL seg6:0x7f5f-0x7fbd]
            for (int k = 0; k < 3; ++k) {
                if (g < 2) g = 2;
                g += ldiv(Q - mul(g, g), mul(g, 2));
            }
            return g;
        };
        const i32 dc = lo16(p.c6c - p.d2e);
        auto depth_level = [&](int yi) { return ldiv(mul(L2, mul(lo16(yi - p.d2e), lo16(yi - p.d2e))), mul(dc, dc)); };
        // The side [BYTES ISPACE.DLL seg8:0x3b62-0x3dd3].
        for (int x = 0; static_cast<i32>(x) < sxm; x = lo16(x + t)) {
            int y[2];
            for (int k = 0; k < 2; ++k) {
                const i32 h = ldiv(sxm, 2);
                const i32 X0 = x;
                const i32 q1 = ldiv(mul(X0 - h, Ry[k]), R);
                const i32 q2 = ldiv(mul(h - X0, Ry[k]), R);
                const i32 Q = mul(q1, q2) + mul(Ry[k], Ry[k]);
                const i32 g = labs32(Ry[k]) - labs32(ldiv(mul(X0 - h, Ry[k]), mul(R, 2)));
                y[k] = lo16(isq(g, Q));
                if (k == 0 ? bot_m > bot_f : top_m > top_f) y[k] = lo16(-y[k]);
            }
            const int yi = lo16(ldiv(lo16(mul16(ybf - ybm, y[0])), Ry[0]) + ybm);
            const i32 F = ldiv(mul(mul(R - x, x - R), 128), mul(R, R)) + 192;
            const i32 K = lo16(depth_level(yi));
            i32 v = ldiv(mul(F, K), 128);
            if (v > 0xfff) v = 0xfff;                      // [BYTES ISPACE.DLL seg8:0x3d5d-0x3d6f]
            const int level = lo16(sar32(v, 1));
            brush(shade_colour(hd(), col, peak, level), 0x3d8a);
            blt(left_m + x, top_m + y[1], t, lo16(bot_m - top_m - y[1] + y[0]));
        }
        // The cross [BYTES ISPACE.DLL seg8:0x3dd6-0x3ee0].
        if (f21 == 1) {
            const int lv = sar16(lo16(ldiv(mul(lo16(ybf - p.d2e), 1 << 12), dc)), 4);
            brush(c0(lv), 0x3e1f);
            int A = idiv16(lo16(sxm), 3);
            if ((A & 1) == 0) A = lo16(A + 1);
            int B = idiv16(lo16(sxm), 10);
            if ((B & 1) == 0) B = lo16(B + 1);
            const int s2 = half16(lo16(sxm)), z6 = idiv16(lo16(szpf), 6);
            blt(s2 - half16(B) + left_m, z6 + top_f, B, A);
            blt(s2 - half16(A) + left_m, half16(A) + (z6 - half16(B)) + top_f, A, B);
        }
        // The top cap [BYTES ISPACE.DLL seg8:0x3ee5-0x41f9].
        t = 4;
        if (Ry[1] < 25) t = 3;
        if (Ry[1] < 12) t = 2;
        if (!(top_m > p.d2e)) return;
        int pl = 0, pr = 0;
        for (int j = lo16(-Ry[1]); static_cast<i32>(j) <= Ry[1]; j = lo16(j + t)) {
            const i32 pj = ldiv(mul(j, R), Ry[1]);
            const i32 Q = mul(R, R) - mul(pj, pj);
            const i32 p2 = ldiv(mul(j, R), mul(Ry[1], 2));
            i32 g = labs32(R) - labs32(p2);
            if (!(ldiv(mul(R, 9), 10) >= labs32(p2))) g = 0;
            const int xj = lo16(isq(g, Q));
            const int yi = lo16(ybm - ldiv(lo16(mul16(ybf - ybm, j)), Ry[0]));
            const int level = sar16(lo16(depth_level(yi)), 1);
            brush(shade_colour(hd(), col, peak, level), 0x417d);
            const int cx = half16(left_m + right_m);
            const int nl = lo16(cx - xj), ny = lo16(top_m - j), nr = lo16(nl + lo16(2 * xj));
            blt(nl, ny, lo16(2 * xj), t);
            if (static_cast<i32>(j) > -Ry[1]) {
                for (int k = 1; k < t; ++k) {
                    if (pl > nl) blt(pl, ny + k + t - 1, idiv16(mul16(nl - pl, t - k), t), 1);
                    else if (pl < nl) blt(nl, ny + k, idiv16(mul16(pl - nl, k), t), 1);
                    if (pr > nr) blt(nr, ny + k, idiv16(mul16(pr - nr, k), t), 1);
                    else if (pr < nr) blt(pr, ny + k + t - 1, idiv16(mul16(nr - pr, t - k), t), 1);
                }
            }
            pl = nl, pr = nr;
        }
    }

    // seg8:0x5470 and seg8:0x5632, the line of a type-2 record (s6-scene.md 1.8)
    // [BYTES ISPACE.DLL seg8:0x547f-0x5625 seg8:0x5640-0x596c].
    struct LinePt {
        int x = 0, y = 0, yb = 0;
    };
    LinePt project(i32 X, i32 Y, i32 Z) const {
        LinePt q;
        const i32 s = p.scale(Y);
        q.yb = p.yb(s);
        q.y = lo16(q.yb - lo16(ldiv(mul(Z, s), 1000)));
        q.x = lo16(p.centre(q.yb) + lo16(ldiv(mul(X, s), 1000)));
        return q;
    }
    void line(const Record& r, int col, int peak, int L) {
        const LinePt a = project(r.pos[0], r.pos[1], r.pos[2]);
        const LinePt b = project(r.size[0], r.size[1], r.size[2]);
        // The point of smaller y first, on equal y the smaller x; equal points draw nothing.
        if (a.y < b.y) segment(a.x, a.y, b.x, b.y, col, peak, L, b.yb, a.yb, 2);
        else if (a.y > b.y) segment(b.x, b.y, a.x, a.y, col, peak, L, a.yb, b.yb, 2);
        else if (b.x < a.x) segment(b.x, b.y, a.x, a.y, col, peak, L, a.yb, b.yb, 2);
        else if (b.x > a.x) segment(a.x, a.y, b.x, b.y, col, peak, L, b.yb, a.yb, 2);
    }
    void segment(int x0, int y0, int x1, int y1, int col, int peak, int L, int ybB, int ybA, int w) {
        const int dy = lo16(y1 - y0);
        if (w == 200) return;                              // [BYTES ISPACE.DLL seg8:0x5640-0x5672]
        const i32 L2 = lo16(2 * L);
        const i32 dx = lo16(x1 - x0);
        const int dir = dy < 0 ? -1 : 1;
        const i32 dc = lo16(p.c6c - p.d2e);
        auto lvl = [&](int yi) {
            return sar16(lo16(ldiv(mul(L2, mul(lo16(yi - p.d2e), lo16(yi - p.d2e))), mul(dc, dc))), 1);
        };
        if (dy == 0) {                                     // [BYTES ISPACE.DLL seg8:0x56ac-0x5730]
            brush(shade_colour(hd(), col, peak, lvl(ybA)), 0x56fc);
            blt(x0, y0, dx, w);
        }
        if (w == 100) {                                    // [BYTES ISPACE.DLL seg8:0x5735-0x578d]
            pen(2, 0, 0x574c);
            point(Op::kMoveTo, x0, y0);
            point(Op::kLineTo, lo16(dx + x0), lo16(dy + y0));
            return;
        }
        for (i32 a = 0; a < abs16(dy); a += 5) {          // [BYTES ISPACE.DLL seg8:0x5790-0x593b]
            const int yi = dir == 1 ? lo16(ybA - ldiv(mul(lo16(ybA - ybB), a), dy))
                                    : lo16(ldiv(mul(lo16(ybA - ybB), a), dy) + ybA);
            pen(w, shade_colour(hd(), col, peak, lvl(yi)), 0x5817);
            if (dir == 1) {
                if (a == 0) point(Op::kMoveTo, lo16(ldiv(mul(dx, 0), dy) + x0), y0);
                const i32 e = std::min<i32>(a + 5, dy);
                point(Op::kLineTo, lo16(ldiv(mul(dx, e), dy) + x0), lo16(lo16(e) + y0));
            } else {
                if (a == 0) point(Op::kMoveTo, lo16(x0 - ldiv(mul(dx, 0), dy)), y0);
                const i32 e = std::max<i32>(-5 - a, dy);
                point(Op::kLineTo, lo16(ldiv(mul(dx, e), dy) + x0), lo16(lo16(e) + y0));
            }
        }
    }
    void pen(int width, std::uint32_t colour, std::uint16_t site) {
        Op o;
        o.kind = Op::kPen;
        o.width = width;
        o.colour = colour;
        o.record = index;
        o.site = site;
        out.ops.push_back(o);
    }
    void point(Op::Kind k, int x, int y) {
        Op o;
        o.kind = k;
        o.x = x, o.y = y;
        o.record = index;
        out.ops.push_back(o);
    }
};

// The compatibility view (see Prim) [DESIGN].
std::vector<Prim> prims_of(const std::vector<Op>& ops) {
    std::vector<Prim> v;
    std::uint32_t brush = 0, pen = 0;
    bool solid = false;
    int pw = 0, cx = 0, cy = 0;
    auto rgb = [](std::uint32_t c) { return Rgb{lo8(static_cast<i32>(c)), lo8(static_cast<i32>(c >> 8)), lo8(static_cast<i32>(c >> 16))}; };
    for (const Op& o : ops) {
        switch (o.kind) {
        case Op::kBrush: brush = o.colour, solid = true; break;
        case Op::kPattern:
        case Op::kStock: solid = false; break;
        case Op::kPatBlt:
            if (solid && o.rop == kPatCopy && o.w > 0 && o.h > 0) {
                Prim q;
                q.kind = Prim::kFill;
                q.x = o.x, q.y = o.y, q.w = o.w, q.h = o.h, q.c = rgb(brush), q.record = o.record;
                v.push_back(q);
            }
            break;
        case Op::kPen: pen = o.colour, pw = o.width; break;
        case Op::kMoveTo: cx = o.x, cy = o.y; break;
        case Op::kLineTo: {
            Prim q;
            q.kind = Prim::kLine;
            q.x = cx, q.y = cy, q.x1 = o.x, q.y1 = o.y, q.width = pw, q.c = rgb(pen), q.record = o.record;
            v.push_back(q);
            cx = o.x, cy = o.y;
            break;
        }
        default: break;
        }
    }
    return v;
}

}  // namespace

// Ordinal 32 [BYTES ISPACE.DLL seg8:0x0-0x376 seg8:0x15ec-0x1b5c].
Render render(const fmt::Scene6600& raw, const Inputs& in) {
    Render out;
    Scene s = decode(raw);
    // The override words [BYTES ISPACE.DLL seg8:0x87-0xb3].
    if (in.ox != 0 || in.oy != 0) {
        put_dword(s.header.raw.data(), 0x5c, in.ox);
        put_dword(s.header.raw.data(), 0x60, in.oy);
        s.header.d5c = in.ox;
        s.header.d60 = in.oy;
    }
    // The Z delta: body word +0x51 (record 2's Z) when the count exceeds 2 and body byte +0x6b
    // (record 2's slot) is 0xec [BYTES ISPACE.DLL seg8:0xb3-0xcc].
    if (s.records.size() > 2 && s.records[2].raw[0x1f] == 0xec) {
        Record& r2 = s.records[2];
        r2.pos[2] = lo16(r2.pos[2] + in.z_delta);
        r2.raw[5] = lo8(r2.pos[2]);
        r2.raw[6] = lo8(r2.pos[2] >> 8);
    }
    out.proj = projection(s.header, in.W, in.H, in.cx, in.cy, in.prev_d2c, in.prev_d2e);
    out.devices = in.devices;  // seg8:0x5c86 [BYTES ISPACE.DLL seg8:0x23b]
    out.ramps = shade_ramps(s.header);
    if (!s.records.empty()) s.records[0].resolved = true;
    out.max_slot = solve(s, out.proj);
    out.slots.assign(static_cast<std::size_t>(std::max(out.max_slot + 1, 0)), Rect{});
    // PatBlt(dc, 0, 0, W, H, BLACKNESS) on the new bitmap [BYTES ISPACE.DLL seg8:0x307-0x31b].
    {
        Op o;
        o.kind = Op::kPatBlt;
        o.w = in.W, o.h = in.H, o.rop = kBlackness;
        out.ops.push_back(o);
    }
    Drawer d{s, out.proj, in, out, 0, {}};
    const Header& h = s.header;
    // The 40-byte buffer [bp-0x30], empty before its first copy (ADR 0016 decision 1) [DESIGN]; in
    // the five scenes every record that prints text copies its own first.
    std::string text;
    // The draw loop: records 1 ... n - 1 in index order [BYTES ISPACE.DLL seg8:0x1afb-0x1b42].
    for (std::size_t i = 1; i < s.records.size(); ++i) {
        const Record& r = s.records[i];
        d.index = static_cast<int>(i);
        if (r.colour < 0 && r.slot < 0) continue;
        const int col = r.colour, peak = peak_of(h, col), L = level_of(h, r);
        switch (r.type) {
        case 1: {
            // The text [BYTES ISPACE.DLL seg8:0x1b3b-0x1b59 seg8:0x1603-0x1634 seg8:0x167a-0x1699].
            if (r.string_id > 1) {
                text = in.strings ? clip(in.strings(r.string_id), 39) : std::string();
            } else if (r.string_id < -1) {
                switch (r.string_id) {
                case -13: text = cpu_text(in.devices.c70); break;  // DGROUP:0xd1c
                case -12: text = in.devices.video; break;  // DGROUP:0x1b34 (lstrcpy, no limit)
                case -11: text = in.devices.sound; break;  // DGROUP:0x1b8e
                case -10: text = in.devices.net; break;    // DGROUP:0x1be8
                default: break;
                }
            }
            bool draw = true;  // [bp-0x32] [BYTES ISPACE.DLL seg8:0x1634-0x16a2]
            if (r.slot < -1) draw = condition_holds(h, r.slot, in.devices);
            if (col >= 0 && draw) {  // [BYTES ISPACE.DLL seg8:0x16a7-0x181d]
                d.box(r.pos[0], r.pos[1], r.size[1], r.size[0], r.pos[2], r.size[2], r.f21, r.f22, col, peak, L,
                      text, r.f24, r.f25, r.f23);
            } else if (draw) {       // colour < 0, slot >= -1 [BYTES ISPACE.DLL seg8:0x17bc-0x181d]
                d.box(r.pos[0], r.pos[1], r.size[1], r.size[0], r.pos[2], r.size[2], 0, 0, -1, -1, -1,
                      std::string(), r.f24, r.f25, r.f23);
            }
            // The slot [BYTES ISPACE.DLL seg8:0x1820-0x1892].
            if (r.slot >= 0 && static_cast<std::size_t>(r.slot) < out.slots.size())
                out.slots[static_cast<std::size_t>(r.slot)] = {d.d30.l, d.d30.t, d.d30.r, lo16(d.d30.b + 1)};
            break;
        }
        case 3:  // [BYTES ISPACE.DLL seg8:0x1895-0x192a]
            d.cylinder(r.pos[0], r.pos[1], r.pos[2], r.size[0], r.size[1], r.size[2], col, peak, L, r.f21);
            break;
        case 2:  // seg8:0x5470 twice [BYTES ISPACE.DLL seg8:0x1a58-0x1af8]
            d.line(r, col, peak, L);
            break;
        default:
            // Types 4 (ISPACE.DLL seg8:0x50ec) and 5 (ISPACE.DLL seg8:0x4206) draw nothing here
            // [OPEN: reading the two routines settles them; only scenes 110 (state 8) and 115 use
            // them, scenes 100 and 105 have none]. Any other type draws nothing in the bytes
            // [BYTES ISPACE.DLL seg8:0x189d seg8:0x1935 seg8:0x19ca seg8:0x1a62].
            break;
        }
    }
    out.prims = prims_of(out.ops);
    return out;
}

Render render(const fmt::Scene6600& raw, int W, int H, int cx, int cy, const Conditions& c) {
    Inputs in;
    in.W = W;
    in.H = H;
    in.cx = cx;
    in.cy = cy;
    in.devices = c;
    return render(raw, in);
}

}  // namespace is::desk
