// isview part 1 of 6: look, shared state and helpers. Included once, by main.cpp.

namespace {

namespace fs = std::filesystem;
namespace isa = is::assets;
namespace isv = is::isview;
using namespace is::fmt;
using isv::ModuleInfo;
using isv::ascii_ieq;
using isv::id_label;
using isv::named_id;
using isv::named_type;
using isv::string_count;
using isv::type_label;

// ---- look

constexpr COLORREF kBg = RGB(0x10, 0x18, 0x28);
constexpr std::uint32_t kBgRgb = 0x101828;
constexpr COLORREF kPanel = RGB(0x16, 0x20, 0x34);
constexpr COLORREF kSelBg = RGB(0x2A, 0x48, 0x78);
constexpr COLORREF kText = RGB(0xD8, 0xE0, 0xF0);
constexpr COLORREF kDim = RGB(0x80, 0x90, 0xA8);
constexpr COLORREF kAccent = RGB(0xF0, 0xC0, 0x40);
constexpr COLORREF kGood = RGB(0x78, 0xD8, 0x88);
constexpr COLORREF kBad = RGB(0xF0, 0x78, 0x68);
constexpr COLORREF kGrid = RGB(0xFF, 0xD0, 0x40);
// Transparent pixels in the bitmap and icon views: a checkerboard of 4-pixel cells.
constexpr std::uint32_t kCheckA = 0x2A3346, kCheckB = 0x3C4862;

constexpr int kTreeWidth = 330;
constexpr int kIndent = 16;      // tree indentation per level
constexpr int kGlyphLeft = 6;    // the tree's +/- box, relative to the indentation
constexpr int kGlyphWidth = 14;
constexpr UINT_PTR kAnimTimer = 1;
constexpr UINT kAnimMs = 160;
constexpr UINT kMsgToggle = WM_APP + 1;  // wParam: node to expand or collapse
constexpr int kMaxZoom = 8;
constexpr int kMaxFrames = 64;

// IGRAPHIC.DLL #4000 60 is the Transformer, a 4-frame strip [docs/re/resources.md].
constexpr std::uint16_t kXformSprite = 60;
constexpr int kXformFrames = 4;
// ENG_LANG.DLL STRING 2338 is the main window's title; MENU 101 and MAINACCEL are the main
// window's menu bar and accelerators (v0).
constexpr std::uint16_t kTitleString = 2338;
constexpr std::uint16_t kMainMenu = 101;
constexpr const char* kMainAccel = "MAINACCEL";
// A ship's initial weapon n is ENG_LANG.DLL STRING 186 + n (src/formats/ships.h).
constexpr std::uint16_t kWeaponString = 186;

enum : int {
    kIdTree = 100, kIdReport, kIdCanvas, kIdSearch, kIdButton, kIdButton2, kIdZoom, kIdFrames,
};

// ---- text (ADR 0003; the conversions are in text.h)

static_assert(sizeof(wchar_t) == sizeof(char16_t), "Win32 wide strings are UTF-16");

std::wstring wide(const std::u16string& u) { return std::wstring(u.begin(), u.end()); }
std::u16string u16(const std::wstring& w) { return std::u16string(w.begin(), w.end()); }

// cp1252 to UTF-16, at the Win32 call.
std::wstring widen(const std::string& s) { return wide(cp1252_to_utf16(s)); }

// An exception's what(): UTF-8 paths and cp1252 names (text.h message_to_utf16).
std::wstring message(const std::string& what) { return wide(isv::message_to_utf16(what)); }

// UTF-16 to cp1252; nullopt when a character has no cp1252 byte. File names only.
std::optional<std::string> narrow(const std::wstring& w) { return isv::cp1252_from_utf16(u16(w)); }

// UTF-16 to UTF-8: standard error is the port's own output (ADR 0003, decision 5).
std::string utf8(const std::wstring& w) { return isv::utf8_from_utf16(u16(w)); }

std::wstring num(long long v) { return std::to_wstring(v); }

std::wstring hex(std::uint32_t v, int digits = 4) {
    wchar_t b[24];
    std::swprintf(b, 24, L"0x%0*X", digits, static_cast<unsigned>(v));
    return b;
}

// One line of a list: control characters, DEL and the bytes cp1252 leaves undefined shown as
// escapes (text.h escape_controls).
std::wstring escaped(const std::wstring& s) { return wide(isv::escape_controls(u16(s))); }

// A character drawn as itself (text.h printable).
bool printable(wchar_t c) { return isv::printable(static_cast<char16_t>(c)); }

std::wstring lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], static_cast<DWORD>(s.size()));
    return s;
}

std::wstring dims(const Dib& d) {
    return num(d.width) + L" \u00D7 " + num(d.height) + L", " + num(d.bpp) + L" bpp";
}

// ---- resources

// std_type_name, type_label, id_label, named_type and named_id are in dump_json.h.

bool same_type(const NeResource& a, const NeResource& b) {
    return a.type == b.type && a.type_name == b.type_name;
}

// The integer type, or 0 for a named one.
std::uint16_t int_type(const NeResource& r) { return named_type(r) ? 0 : r.type; }

// ---- GDI

// A 32 bpp top-down DIB section.
class Image {
public:
    Image() = default;
    Image(const std::vector<std::uint32_t>& px, int w, int h) : w_(w), h_(h) {
        if (w <= 0 || h <= 0 || px.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(h))
            return;
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;  // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        bmp_ = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bmp_ && bits) std::memcpy(bits, px.data(), static_cast<std::size_t>(w) * h * sizeof px[0]);
    }
    Image(Image&& o) noexcept : bmp_(o.bmp_), w_(o.w_), h_(o.h_) { o.bmp_ = nullptr; }
    Image& operator=(Image&& o) noexcept {
        if (this != &o) {
            reset();
            bmp_ = o.bmp_;
            w_ = o.w_;
            h_ = o.h_;
            o.bmp_ = nullptr;
        }
        return *this;
    }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    ~Image() { reset(); }

    void reset() {
        if (bmp_) DeleteObject(bmp_);
        bmp_ = nullptr;
    }
    HBITMAP get() const { return bmp_; }
    int w() const { return w_; }
    int h() const { return h_; }
    explicit operator bool() const { return bmp_ != nullptr; }

private:
    HBITMAP bmp_ = nullptr;
    int w_ = 0, h_ = 0;
};

enum class Backdrop : std::uint8_t {
    kBackground,  // transparent pixels take the window colour (v0's sprite gallery)
    kChecker,     // transparent pixels show a checkerboard
};

std::uint32_t checker(int x, int y) { return (((x >> 2) + (y >> 2)) & 1) ? kCheckA : kCheckB; }

void checker_transparent(std::vector<std::uint32_t>& px, int w) {
    for (std::size_t i = 0; i < px.size(); ++i)
        if ((px[i] >> 24) == 0)
            px[i] = 0xFF000000u | checker(static_cast<int>(i % static_cast<std::size_t>(w)),
                                          static_cast<int>(i / static_cast<std::size_t>(w)));
}

// The viewer draws palette index 15 of the 4 bpp sprite DIBs (#4000) transparent, as v0 did
// [DESIGN]. docs/re/resources.md finds the bytes consistent with a transparent index 15; how
// the game masks sprites is [OPEN].
int sprite_transparent(const Dib& d) { return d.bpp == 4 ? 15 : -1; }

Image dib_image(const Dib& d, int transparent, Backdrop b) {
    auto px = to_bgra(d, transparent, kBgRgb);
    if (b == Backdrop::kChecker) checker_transparent(px, d.width);
    return Image(px, d.width, d.height);
}

Image icon_image(const Icon& ic) {
    auto px = icon_to_bgra(ic);
    checker_transparent(px, ic.image.width);
    return Image(px, ic.image.width, ic.image.height);
}

// ---- model

// ModuleInfo is in dump_json.h.

enum class NodeKind : std::uint8_t {
    kOverview, kIdentify, kShips, kShip, kBodies, kBody, kModule, kType, kResource,
};

struct Node {
    NodeKind kind = NodeKind::kOverview;
    int level = 0;
    int parent = -1;
    bool has_children = false;
    bool expanded = false;
    std::wstring label;
    std::wstring note;                // dim text after the label
    std::wstring key;                 // what --select matches, compared without case
    int module = -1;                  // index into App::modules
    const NeResource* res = nullptr;  // kResource; kType: the type's first resource
    int index = -1;                   // kShip, kBody: the record index
};

enum class ViewKind : std::uint8_t {
    kMessage, kOverview, kIdentify, kModule, kTypeTable, kStrings, kDialog, kMenu, kAccel,
    kBitmap, kGallery, kIcons, kWave, kVersion, kShips, kShip, kBodies, kBody, kData, kPalette,
};

bool uses_report(ViewKind k) {
    switch (k) {
    case ViewKind::kIdentify: case ViewKind::kModule: case ViewKind::kTypeTable:
    case ViewKind::kStrings: case ViewKind::kDialog: case ViewKind::kMenu: case ViewKind::kAccel:
    case ViewKind::kVersion: case ViewKind::kShips: case ViewKind::kShip: case ViewKind::kBodies:
    case ViewKind::kBody: case ViewKind::kData:
        return true;
    default:
        return false;
    }
}

bool uses_canvas(ViewKind k) {
    switch (k) {
    case ViewKind::kMessage: case ViewKind::kOverview: case ViewKind::kBitmap:
    case ViewKind::kGallery: case ViewKind::kIcons: case ViewKind::kWave: case ViewKind::kPalette:
    case ViewKind::kShip: case ViewKind::kBody:
        return true;
    default:
        return false;
    }
}

// The views whose canvas shows a picture with the zoom and frame controls.
bool uses_picture(ViewKind k) {
    return k == ViewKind::kBitmap || k == ViewKind::kShip || k == ViewKind::kBody;
}

struct Column {
    std::wstring title;
    int chars = 0;  // left edge, in average character widths
};

struct Line {
    std::wstring text;  // columns separated by '\t'
    COLORREF color = kText;
    int node = -1;      // the tree node a double click selects
};

struct Tile {  // a gallery cell or an icon image
    Image img;
    std::wstring label;
    int node = -1;
};

struct View {
    ViewKind kind = ViewKind::kMessage;
    int node = -1;
    std::wstring title, subtitle;
    // the report
    std::vector<Column> columns;
    std::vector<Line> lines;
    bool mono = false;
    // the canvas
    std::vector<std::wstring> text;  // lines drawn above the picture
    Image img;                       // the bitmap, sprite or ship body
    Dib dib;                         // its size and palette
    int transparent = -1;            // palette index drawn transparent
    int zoom = 1, frames = 1;
    std::vector<Tile> tiles;         // the gallery or the icon images
    isa::Sound sound;
    bool has_sound = false;
    Palette5500 palette{};
    // what the button acts on
    std::vector<std::uint8_t> dialog_tmpl;  // Win32 DLGTEMPLATE
    std::vector<MenuItem16> menu;
};

struct Sprite {
    std::uint16_t id = 0;
    Image img;
};

struct Label {
    int x = 0, y = 0;
    std::wstring text;
};

struct App {
    HINSTANCE inst = nullptr;
    HWND main = nullptr, tree = nullptr, report = nullptr, canvas = nullptr;
    HWND search = nullptr, button = nullptr, button2 = nullptr, zoom = nullptr, frames = nullptr;
    HWND modeless = nullptr;
    HFONT font = nullptr, bold = nullptr, small = nullptr, mono = nullptr;
    HBRUSH bg_brush = nullptr, panel_brush = nullptr, dim_brush = nullptr;
    int line_h = 16, char_w = 7, mono_w = 7, small_h = 12;
    HMENU menu = nullptr;
    HACCEL accel = nullptr;
    WNDPROC tree_proc = nullptr, report_proc = nullptr;
    bool filling = false;  // set while the program writes into its own controls

    fs::path dir;
    std::unique_ptr<isa::AssetStore> store;
    std::vector<ModuleInfo> modules;
    std::optional<isa::Identification> ident;
    std::string ident_error;
    std::string version;  // INSPACE.EXE's FileVersion string
    std::optional<isa::Ships> ships;
    std::string ships_error;
    std::wstring title;   // ENG_LANG.DLL STRING 2338
    std::wstring status = L"Menu and accelerator commands appear here.";
    std::vector<std::wstring> load_notes;  // what the start-up could not load

    std::vector<Node> nodes;
    std::vector<int> visible;  // tree rows: node indices
    View view;
    std::vector<Line> lines;   // report rows, after the search filter
    std::vector<Label> labels;  // toolbar captions, drawn by the main window
    int colhdr_y = -1, report_x = 0, report_w = 0;

    std::vector<Sprite> sprites;  // IGRAPHIC.DLL #4000, for the overview
    const Sprite* xform = nullptr;
    int frame = 0;
    std::vector<std::uint8_t> playing;  // the WAV that PlaySoundW reads

    std::vector<std::pair<RECT, int>> hits;  // canvas rectangles (content coordinates) -> node
    int scroll_x = 0, scroll_y = 0, ext_w = 0, ext_h = 0;
} g;

void select_node(int n);
void set_status(const std::wstring& s);

}  // namespace
