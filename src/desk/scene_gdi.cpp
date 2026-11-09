#ifdef _WIN32
#include "desk/scene_gdi.h"

#include "gfx/guard.h"

#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

#include <mmsystem.h>

#include "assets/asset_store.h"
#include "formats/records.h"
#include "gfx/gfx.h"

namespace is::desk {

namespace {

std::wstring wide1252(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// The objects the replay creates, deleted when the next of their kind replaces them and at the
// end; the DC's own objects come back on return [DESIGN] (the bytes select and delete each
// brush and pen around its use, which leaves the same pixels).
class Replay {
public:
    Replay(HDC dc, const SceneGdi& with) : dc_(dc), with_(with) {
        brush0_ = GetCurrentObject(dc_, OBJ_BRUSH);
        pen0_ = GetCurrentObject(dc_, OBJ_PEN);
        font0_ = GetCurrentObject(dc_, OBJ_FONT);
        align0_ = GetTextAlign(dc_);
        bkmode0_ = GetBkMode(dc_);
        bk0_ = GetBkColor(dc_);
        text0_ = GetTextColor(dc_);
        stretch0_ = GetStretchBltMode(dc_);
        MoveToEx(dc_, 0, 0, &pos0_);
    }
    ~Replay() {
        SelectObject(dc_, brush0_);
        SelectObject(dc_, pen0_);
        SelectObject(dc_, font0_);
        if (brush_) DeleteObject(brush_);
        if (pen_) DeleteObject(pen_);
        SetTextAlign(dc_, align0_);
        SetBkMode(dc_, bkmode0_);
        SetBkColor(dc_, bk0_);
        SetTextColor(dc_, text0_);
        SetStretchBltMode(dc_, stretch0_);
        MoveToEx(dc_, pos0_.x, pos0_.y, nullptr);
    }
    Replay(const Replay&) = delete;
    Replay& operator=(const Replay&) = delete;

    void run(const Op& o) {
        switch (o.kind) {
        case Op::kBrush: set_brush(CreateSolidBrush(colour(o.colour))); break;
        case Op::kPattern: {
            HBITMAP b = with_.pattern ? with_.pattern(o.pattern) : nullptr;
            HBRUSH h = b ? CreatePatternBrush(b) : nullptr;
            if (h) {
                set_brush(h);
            } else {
                // f21 >= 100 other than 100 and 120 makes CreatePatternBrush of the unwritten
                // [bp-0x76]; the port leaves the DC's own brush, as a failed CreatePatternBrush
                // does (see SceneGdi::pattern) [DESIGN ADR 0016 decision 1] (review M7).
                if (o.pattern == Pattern::kNone) {
                    static bool once = false;
                    gfx::guard_todo(once, "scene pattern f21 >= 100 other than 100 / 120: the unwritten bitmap",
                                    "ISPACE.DLL seg8:0x3508");
                }
                set_brush(nullptr);
            }
            break;
        }
        case Op::kStock: set_stock(GetStockObject(o.stock)); break;
        case Op::kPatBlt: PatBlt(dc_, o.x, o.y, o.w, o.h, o.rop); break;
        case Op::kPen: set_pen(CreatePen(PS_SOLID, o.width, colour(o.colour))); break;
        case Op::kMoveTo: MoveToEx(dc_, o.x, o.y, nullptr); break;
        case Op::kLineTo: LineTo(dc_, o.x, o.y); break;
        case Op::kText: text(o); break;
        case Op::kFrame:
            for (const Op& f : frame_ops(o)) run(f);
            break;
        }
    }

private:
    COLORREF colour(std::uint32_t c) const { return with_.colour ? with_.colour(c) : static_cast<COLORREF>(c); }
    void set_brush(HBRUSH b) {
        SelectObject(dc_, b ? static_cast<HGDIOBJ>(b) : brush0_);
        if (brush_) DeleteObject(brush_);
        brush_ = b;
    }
    void set_stock(HGDIOBJ stock) {
        SelectObject(dc_, stock);
        if (brush_) DeleteObject(brush_);
        brush_ = nullptr;
    }
    void set_pen(HPEN p) {
        SelectObject(dc_, p ? static_cast<HGDIOBJ>(p) : pen0_);
        if (pen_) DeleteObject(pen_);
        pen_ = p;
    }
    // ISPACE.DLL seg8:0x5978's GDI calls (s6-scene.md 1.9) [BYTES ISPACE.DLL seg8:0x59ad-0x5a14
    // seg8:0x5a29-0x5a7d seg8:0x5ae2-0x5b30]: CreateFont(h, w, 0, 0, weight, 0, 0, 0, 0, 0, 0,
    // PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS, "Arial"), SetBkColor(0), SetBkMode(TRANSPARENT),
    // SetTextAlign, SetTextColor, TextOut, then the old colours and font and DeleteObject.
    void text(const Op& o) {
        HFONT f = CreateFontW(o.font_h, o.font_w, 0, 0, o.weight, 0, 0, 0, 0, 0, 0, 2, 0x22, L"Arial");
        HGDIOBJ of = SelectObject(dc_, f);
        const COLORREF obk = SetBkColor(dc_, 0);
        SetBkMode(dc_, TRANSPARENT);
        if (o.align >= 0) SetTextAlign(dc_, static_cast<UINT>(o.align));
        const COLORREF otext = SetTextColor(dc_, colour(o.colour));
        const std::wstring w = wide1252(o.text);
        if (!w.empty()) {
            // ExtTextOutW with the extent's advances (ADR 0007 Consequences) [DESIGN].
            std::vector<INT> fit(w.size()), dx(w.size());
            SIZE sz{};
            if (GetTextExtentExPointW(dc_, w.c_str(), static_cast<int>(w.size()), 0, nullptr, fit.data(), &sz)) {
                for (std::size_t i = 0; i < w.size(); ++i) dx[i] = fit[i] - (i ? fit[i - 1] : 0);
                ExtTextOutW(dc_, o.x, o.y, 0, nullptr, w.c_str(), static_cast<UINT>(w.size()), dx.data());
            } else {
                ExtTextOutW(dc_, o.x, o.y, 0, nullptr, w.c_str(), static_cast<UINT>(w.size()), nullptr);
            }
        }
        SetTextColor(dc_, otext);
        SetBkColor(dc_, obk);
        SelectObject(dc_, of);
        DeleteObject(f);
    }

    HDC dc_;
    const SceneGdi& with_;
    HGDIOBJ brush0_ = nullptr, pen0_ = nullptr, font0_ = nullptr;
    HBRUSH brush_ = nullptr;
    HPEN pen_ = nullptr;
    UINT align0_ = 0;
    int bkmode0_ = 0, stretch0_ = 0;
    COLORREF bk0_ = 0, text0_ = 0;
    POINT pos0_{};
};

}  // namespace

void draw_scene(HDC dc, const Render& r, const SceneGdi& with) {
    if (!dc) return;
    Replay rp(dc, with);
    SetStretchBltMode(dc, COLORONCOLOR);  // SetStretchBltMode(dc, 3) [BYTES ISPACE.DLL seg8:0x302]
    for (const Op& o : r.ops) rp.run(o);
    GdiFlush();
}

void draw_scene(gfx::Gfx& g, HDC dc, const Render& r, const std::function<void(const std::string&, const char*)>& todo) {
    if (!dc) return;
    // The PALETTERGB colours in drawing order through command 6, then command 3 (ADR 0007)
    // [DESIGN]; ordinal 34's plain RGB greys are static colours and allocate nothing.
    std::set<std::uint32_t> seen;
    bool metal = false, g76 = false;
    for (const Op& o : r.ops) {
        if (o.kind == Op::kPattern) {
            metal = metal || o.pattern == Pattern::kMetal;
            g76 = g76 || o.pattern == Pattern::kGraphic76;
        }
        if (o.kind != Op::kBrush && o.kind != Op::kPen && o.kind != Op::kText) continue;
        if ((o.colour & 0xff000000u) != 0x02000000u || !seen.insert(o.colour).second) continue;
        g.palette_colour(static_cast<int>(o.colour & 0xff), static_cast<int>((o.colour >> 8) & 0xff),
                         static_cast<int>((o.colour >> 16) & 0xff));
    }
    g.palette_animate();
    gfx::ImagePtr metal_img, g76_img;
    try {
        if (metal) metal_img = g.load_bitmap("METAL");  // ISPACE.DLL DGROUP:0x31
    } catch (const std::exception& e) {
        if (todo) todo(std::string("scene METAL: ") + e.what(), "ISPACE.DLL seg8:0x34d1");
    }
    try {
        if (g76) g76_img = g.load_graphic(76, 0, -1);
    } catch (const std::exception& e) {
        if (todo) todo(std::string("scene #4000 76: ") + e.what(), "ISPACE.DLL seg8:0x34ef");
    }
    if (metal && !metal_img && todo) todo("scene METAL not loaded", "ISPACE.DLL seg8:0x34d1");
    if (g76 && !g76_img && todo) todo("scene #4000 76 not loaded", "ISPACE.DLL seg8:0x34ef");
    SceneGdi with;
    with.colour = [&g](std::uint32_t c) { return g.dc_colour(static_cast<COLORREF>(c)); };
    with.pattern = [&](Pattern p) -> HBITMAP {
        if (p == Pattern::kMetal && metal_img) return metal_img->bitmap();
        if (p == Pattern::kGraphic76 && g76_img) return g76_img->bitmap();
        return nullptr;
    };
    draw_scene(dc, r, with);
}

Render render_scene(const assets::AssetStore& store, int id, int W, int H, int cx, int cy,
                    std::function<std::string(int)> strings, int ox, int oy, int z_delta) {
    // ISPACE.DLL seg8:0x5b76 loads the resource [BYTES ISPACE.DLL seg8:0x44].
    const fmt::Scene6600 raw = fmt::load_scene6600(store.module(assets::kGraphicsDll), static_cast<std::uint16_t>(id));
    Inputs in;
    in.W = W;
    in.H = H;
    in.cx = cx;
    in.cy = cy;
    in.ox = ox;
    in.oy = oy;
    in.z_delta = z_delta;
    in.depth = 8;  // the port's display (ADR 0007 decision 1) [DESIGN]
    in.devices = read_devices();  // seg8:0x5c86 on every call [BYTES ISPACE.DLL seg8:0x23b]
    in.strings = std::move(strings);
    return render(raw, in);
}

Conditions read_devices() {
    DeviceSource s;
    char dir[MAX_PATH + 1] = {};
    UINT n = GetWindowsDirectoryA(dir, MAX_PATH);  // the bytes' buffer is 0x6e [DESIGN: MAX_PATH]
    s.windows_dir = n > 0 && n < MAX_PATH ? std::string(dir, n) : std::string();
    n = GetSystemDirectoryA(dir, MAX_PATH);
    s.system_dir = n > 0 && n < MAX_PATH ? std::string(dir, n) : std::string();
    s.profile = [](const std::string& section, const std::string& key, const std::string& def, int size,
                   const std::string& file) {
        std::vector<char> buf(static_cast<std::size_t>(size > 0 ? size : 1), '\0');
        GetPrivateProfileStringA(section.c_str(), key.c_str(), def.c_str(), buf.data(), static_cast<DWORD>(buf.size()),
                                 file.c_str());
        buf.back() = '\0';
        return std::string(buf.data());
    };
    s.wave_devs = waveOutGetNumDevs();  // [BYTES ISPACE.DLL seg8:0x5ddf]
    if (s.wave_devs != 0) {
        // A failed call leaves the Win16 stack buffer as it was, unwritten; the port's is zero
        // [DESIGN ADR 0016 decision 1] (review M7).
        WAVEOUTCAPSA caps{};
        if (waveOutGetDevCapsA(0, &caps, sizeof caps) != MMSYSERR_NOERROR) {  // [BYTES ISPACE.DLL seg8:0x5df9]
            static bool once = false;
            gfx::guard_todo(once, "waveOutGetDevCaps failed: the sound card's caps read 0", "ISPACE.DLL seg8:0x5df9");
        }
        s.wave_support = caps.dwSupport;
        s.wave_channels = caps.wChannels;
    }
    s.win_flags = kWinFlagsPort;  // GetWinFlags [BYTES ISPACE.DLL seg8:0x5f4c] [DESIGN]
    return devices_from(s);
}

}  // namespace is::desk
#endif
