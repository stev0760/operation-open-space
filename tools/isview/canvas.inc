// isview part 6 of 6: the canvas, a scrolling child window that draws the picture views:
// v0's overview, galleries, bitmaps with zoom, frame grid and palette, icons, waveforms and
// the #5500 palette. Painters draw in content coordinates and record the extent they used,
// which sets the scroll bars, and the rectangles a click selects. Included once, by main.cpp.

namespace {

struct Painter {
    HDC dc = nullptr;
    HDC src = nullptr;
    HGDIOBJ src_old = nullptr;
    int width = 0;  // the visible width
    int ext_w = 0, ext_h = 0;

    void note(int right, int bottom) {
        ext_w = std::max(ext_w, right);
        ext_h = std::max(ext_h, bottom);
    }
    int text(int x, int y, const std::wstring& s, COLORREF c, HFONT f = nullptr) {
        if (f) SelectObject(dc, f);
        SetTextColor(dc, c);
        TextOutW(dc, x, y, s.c_str(), static_cast<int>(s.size()));
        SIZE sz{};
        GetTextExtentPoint32W(dc, s.c_str(), static_cast<int>(s.size()), &sz);
        if (f) SelectObject(dc, g.font);
        note(x + sz.cx, y + sz.cy);
        return sz.cx;
    }
    void image(const Image& img, int sx, int sw, int x, int y, int w, int h) {
        if (!img || sw <= 0) return;
        const HGDIOBJ prev = SelectObject(src, img.get());
        if (!src_old) src_old = prev;
        StretchBlt(dc, x, y, w, h, src, sx, 0, sw, img.h(), SRCCOPY);
        note(x + w, y + h);
    }
    void fill(int x, int y, int w, int h, COLORREF c) {
        RECT r{x, y, x + w, y + h};
        SetBkColor(dc, c);
        ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &r, nullptr, 0, nullptr);
        note(x + w, y + h);
    }
    void frame(int x, int y, int w, int h, COLORREF c) {
        fill(x, y, w, 1, c);
        fill(x, y + h - 1, w, 1, c);
        fill(x, y, 1, h, c);
        fill(x + w - 1, y, 1, h, c);
    }
    void hit(int x, int y, int w, int h, int node) {
        if (node >= 0) g.hits.push_back({RECT{x, y, x + w, y + h}, node});
    }
};

constexpr int kMargin = 16;

int text_block(Painter& p, int x, int y, const std::vector<std::wstring>& lines, COLORREF c) {
    for (const std::wstring& s : lines) {
        p.text(x, y, s, c);
        y += g.line_h;
    }
    return y;
}

// The tree node of IGRAPHIC.DLL #4000 id, for the overview's clickable sprites.
int sprite_node(std::uint16_t id) {
    static std::map<std::uint16_t, int> cache;
    static bool built = false;
    if (!built) {
        built = true;
        for (int i = 0; i < static_cast<int>(g.nodes.size()); ++i) {
            const Node& n = g.nodes[i];
            if (n.kind == NodeKind::kResource && ascii_ieq(g.modules[n.module].name, isa::kGraphicsDll) &&
                int_type(*n.res) == isa::kSpriteType && !named_id(*n.res))
                cache[n.res->id] = i;
        }
    }
    const auto it = cache.find(id);
    return it == cache.end() ? -1 : it->second;
}

// A picture scaled to fit a w x h box, whole-number zoom when it fits.
void fit(Painter& p, const Image& img, int x, int y, int box_w, int box_h) {
    if (!img) return;
    double k = std::min(static_cast<double>(box_w) / img.w(), static_cast<double>(box_h) / img.h());
    if (k >= 1) k = std::floor(k);
    const int dw = std::max(1, static_cast<int>(img.w() * k)), dh = std::max(1, static_cast<int>(img.h() * k));
    p.image(img, 0, img.w(), x + (box_w - dw) / 2, y + (box_h - dh) / 2, dw, dh);
}

// v0's page.
void paint_overview(Painter& p) {
    const int x0 = kMargin;
    int y = 12;
    p.text(x0, y, L"Operation: Inner Space, read from the original Win16 modules by the port's asset store and readers",
           kText);
    y += g.line_h + 2;
    p.text(x0, y, L"Source: " + g.dir.wstring(), kDim);
    y += g.line_h;
    p.text(x0, y, L"Main window title, ENG_LANG.DLL string 2338: \u201C" + g.title + L"\u201D", kDim);
    y += g.line_h;
    y = text_block(p, x0, y, g.load_notes, kBad);
    y += 16;

    if (g.xform) {
        const Image& img = g.xform->img;
        p.text(x0, y, L"IGRAPHIC.DLL #4000 60: the Transformer, 4 animation frames (animated, \u00D74), and the strip (\u00D72)",
               kText);
        y += g.line_h + 6;
        const int fw = img.w() / kXformFrames;
        p.image(img, (g.frame % kXformFrames) * fw, fw, x0, y, fw * 4, img.h() * 4);
        p.image(img, 0, img.w(), x0 + fw * 4 + 32, y + img.h(), img.w() * 2, img.h() * 2);
        p.hit(x0, y, fw * 4 + 32 + img.w() * 2, img.h() * 4, sprite_node(kXformSprite));
        y += img.h() * 4 + 24;
    }
    if (g.sprites.empty()) return;

    p.text(x0, y,
           L"IGRAPHIC.DLL #4000: all " + num(static_cast<long long>(g.sprites.size())) +
               L" sprite DIBs, 4 bpp with their own palettes, index 15 drawn transparent (a viewer choice); "
               L"click one to open it",
           kText);
    y += g.line_h + 6;
    const int cell = 60, box_w = 54, box_h = 42;
    const int per_row = std::max(1, (p.width - 2 * x0) / cell);
    for (std::size_t i = 0; i < g.sprites.size(); ++i) {
        const Sprite& s = g.sprites[i];
        const int cx = x0 + static_cast<int>(i % static_cast<std::size_t>(per_row)) * cell;
        const int cy = y + static_cast<int>(i / static_cast<std::size_t>(per_row)) * (box_h + 16);
        fit(p, s.img, cx, cy, box_w, box_h);
        p.text(cx + 2, cy + box_h + 1, num(s.id), kDim, g.small);
        p.hit(cx, cy, box_w, box_h + 14, sprite_node(s.id));
    }
}

void paint_gallery(Painter& p) {
    const int x0 = kMargin, y0 = 12;
    const int box_w = 112, box_h = 84, cell_w = 124, cell_h = box_h + g.small_h + 14;
    const int per_row = std::max(1, (p.width - 2 * x0) / cell_w);
    for (std::size_t i = 0; i < g.view.tiles.size(); ++i) {
        const Tile& t = g.view.tiles[i];
        const int cx = x0 + static_cast<int>(i % static_cast<std::size_t>(per_row)) * cell_w;
        const int cy = y0 + static_cast<int>(i / static_cast<std::size_t>(per_row)) * cell_h;
        p.frame(cx - 1, cy - 1, box_w + 2, box_h + 2, kPanel);
        fit(p, t.img, cx, cy, box_w, box_h);
        std::wstring label = t.label;
        if (t.img) label += L"  " + num(t.img.w()) + L"\u00D7" + num(t.img.h());
        p.text(cx + 2, cy + box_h + 3, label, kDim, g.small);
        p.hit(cx, cy, box_w, box_h + g.small_h + 4, t.node);
    }
}

// Colour swatches, 16 to a row, each with its index and RGB value.
int paint_swatches(Painter& p, int x0, int y, const std::vector<std::uint32_t>& colours,
                   int first_index, int transparent) {
    const int box = 24, cell_w = 56, cell_h = box + 2 * g.small_h + 10;
    for (std::size_t i = 0; i < colours.size(); ++i) {
        const int cx = x0 + static_cast<int>(i % 16) * cell_w;
        const int cy = y + static_cast<int>(i / 16) * cell_h;
        const std::uint32_t c = colours[i];
        p.fill(cx, cy, box, box, RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF));
        p.frame(cx - 1, cy - 1, box + 2, box + 2, kDim);
        const int index = first_index + static_cast<int>(i);
        p.text(cx, cy + box + 2, num(index) + (index == transparent ? L" T" : L""),
               index == transparent ? kAccent : kDim, g.small);
        wchar_t rgb[16];
        std::swprintf(rgb, 16, L"%06X", static_cast<unsigned>(c & 0xFFFFFF));
        p.text(cx, cy + box + 2 + g.small_h, rgb, kDim, g.small);
    }
    const int rows = static_cast<int>((colours.size() + 15) / 16);
    return y + rows * cell_h;
}

// The picture views: the text, the image at the zoom with the frame grid, the current frame
// animated and the palette.
void paint_picture(Painter& p) {
    const View& v = g.view;
    const int x0 = kMargin;
    int y = text_block(p, x0, 12, v.text, kDim);
    if (!v.img) return;
    const int z = v.zoom, w = v.img.w(), h = v.img.h();
    const int n = std::clamp(v.frames, 1, std::max(1, w));
    const int fw = w / n;
    y += 4;
    std::wstring grid = L"Zoom \u00D7" + num(z) + L" \u00B7 frame grid: " + num(n) +
                        (n == 1 ? L" frame" : L" frames of " + num(fw) + L" \u00D7 " + num(h));
    if (w % n) grid += L" (the width is not a multiple of " + num(n) + L")";
    p.text(x0, y, grid, kText);
    y += g.line_h + 6;
    if (n > 1) {
        for (int k = 0; k < n; ++k) p.text(x0 + k * fw * z + 2, y, num(k), kGrid, g.small);
        y += g.small_h + 2;
    }
    p.image(v.img, 0, w, x0, y, w * z, h * z);
    p.frame(x0 - 1, y - 1, w * z + 2, h * z + 2, kDim);
    for (int k = 1; k < n; ++k) p.fill(x0 + k * fw * z, y, 1, h * z, kGrid);
    y += h * z + 16;
    if (n > 1) {
        const int f = g.frame % n;
        p.text(x0, y, L"Frame " + num(f) + L" of 0\u2013" + num(n - 1) + L", animated", kText);
        y += g.line_h + 4;
        p.image(v.img, f * fw, fw, x0, y, fw * z, h * z);
        p.frame(x0 - 1, y - 1, fw * z + 2, h * z + 2, kDim);
        y += h * z + 16;
    }
    if (!v.dib.palette.empty()) {
        p.text(x0, y,
               L"Palette: " + num(static_cast<long long>(v.dib.palette.size())) + L" entries" +
                   (v.transparent >= 0 ? L"; T marks the index drawn transparent" : L""),
               kText);
        y += g.line_h + 6;
        paint_swatches(p, x0, y, v.dib.palette, 0, v.transparent);
    }
}

void paint_icons(Painter& p) {
    const int x0 = kMargin;
    int y = 12;
    for (const Tile& t : g.view.tiles) {
        p.text(x0, y, t.label, kText);
        y += g.line_h + 6;
        if (!t.img) continue;
        int x = x0;
        for (int z : {1, 2, 4}) {
            p.text(x, y, L"\u00D7" + num(z), kDim, g.small);
            p.image(t.img, 0, t.img.w(), x, y + g.small_h + 2, t.img.w() * z, t.img.h() * z);
            x += t.img.w() * z + 24;
        }
        p.hit(x0, y, x - x0, g.small_h + 2 + t.img.h() * 4, t.node);
        y += g.small_h + 2 + t.img.h() * 4 + 20;
    }
}

void paint_wave(Painter& p) {
    const View& v = g.view;
    const int x0 = kMargin;
    int y = text_block(p, x0, 12, v.text, kText);
    y += 12;
    const Wave& w = v.sound.wave;
    const int bw = std::max(300, p.width - 3 * kMargin), bh = 180;
    p.fill(x0, y, bw, bh, kPanel);
    p.fill(x0, y + bh / 2, bw, 1, kDim);
    const auto cols = isv::wave_columns(w, v.sound.file.data(), v.sound.file.size(), bw);
    for (int x = 0; x < static_cast<int>(cols.size()); ++x) {
        const int top = y + bh / 2 - cols[x].hi * (bh / 2 - 2) / 32768;
        const int bottom = y + bh / 2 - cols[x].lo * (bh / 2 - 2) / 32768;
        p.fill(x0 + x, top, 1, std::max(1, bottom - top + 1), kGood);
    }
    y += bh + 8;
    const std::size_t frames = w.frames();
    std::wstring caption;
    if (!cols.empty())
        caption = L"First channel, one column per " +
                  num(static_cast<long long>(std::max<std::size_t>(1, frames / static_cast<std::size_t>(bw)))) +
                  L" frames";
    else if (frames == 0)
        caption = L"No waveform: the data chunk holds no whole frame";
    else
        caption = L"No waveform: the view draws PCM whose frames each hold a whole 8-bit or 16-bit sample";
    p.text(x0, y, caption, kDim, g.small);
}

void paint_palette(Painter& p) {
    const int x0 = kMargin;
    std::vector<std::uint32_t> colours;
    for (const Rgb8& c : g.view.palette.colours)
        colours.push_back((std::uint32_t{c.r} << 16) | (std::uint32_t{c.g} << 8) | c.b);
    p.text(x0, 12, L"Labelled with the LOGPALETTE index each colour goes to (11 + k)", kDim);
    paint_swatches(p, x0, 12 + g.line_h + 8, colours, Palette5500::kFirstIndex, -1);
}

// ---- the window

void set_scroll() {
    RECT rc;
    GetClientRect(g.canvas, &rc);
    SCROLLINFO si{};
    si.cbSize = sizeof si;
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMax = std::max(0, g.ext_h - 1);
    si.nPage = static_cast<UINT>(std::max(0L, rc.bottom));
    si.nPos = g.scroll_y;
    SetScrollInfo(g.canvas, SB_VERT, &si, TRUE);
    si.nMax = std::max(0, g.ext_w - 1);
    si.nPage = static_cast<UINT>(std::max(0L, rc.right));
    si.nPos = g.scroll_x;
    SetScrollInfo(g.canvas, SB_HORZ, &si, TRUE);
    si.fMask = SIF_POS;
    GetScrollInfo(g.canvas, SB_VERT, &si);
    g.scroll_y = si.nPos;
    GetScrollInfo(g.canvas, SB_HORZ, &si);
    g.scroll_x = si.nPos;
}

void scroll_to(int bar, int pos) {
    SCROLLINFO si{};
    si.cbSize = sizeof si;
    si.fMask = SIF_ALL;
    GetScrollInfo(g.canvas, bar, &si);
    pos = std::clamp(pos, 0, std::max(0, si.nMax - static_cast<int>(si.nPage) + 1));
    si.fMask = SIF_POS;
    si.nPos = pos;
    SetScrollInfo(g.canvas, bar, &si, TRUE);
    (bar == SB_VERT ? g.scroll_y : g.scroll_x) = pos;
    InvalidateRect(g.canvas, nullptr, FALSE);
}

void on_scroll(int bar, int code) {
    SCROLLINFO si{};
    si.cbSize = sizeof si;
    si.fMask = SIF_ALL;
    GetScrollInfo(g.canvas, bar, &si);
    int pos = si.nPos;
    const int line = g.line_h * 2;
    switch (code) {
    case SB_LINEUP: pos -= line; break;
    case SB_LINEDOWN: pos += line; break;
    case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
    case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
    case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
    case SB_TOP: pos = 0; break;
    case SB_BOTTOM: pos = si.nMax; break;
    default: return;
    }
    scroll_to(bar, pos);
}

void paint_canvas(HWND h) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);
    const int w = std::max(1L, rc.right), hh = std::max(1L, rc.bottom);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP buf = CreateCompatibleBitmap(dc, w, hh);
    const HGDIOBJ old_buf = SelectObject(mem, buf);
    RECT all{0, 0, w, hh};
    FillRect(mem, &all, g.bg_brush);
    SetBkMode(mem, TRANSPARENT);
    SetStretchBltMode(mem, COLORONCOLOR);
    const HGDIOBJ old_font = SelectObject(mem, g.font);
    SetViewportOrgEx(mem, -g.scroll_x, -g.scroll_y, nullptr);

    Painter p;
    p.dc = mem;
    p.src = CreateCompatibleDC(dc);
    p.width = w;
    g.hits.clear();
    switch (g.view.kind) {
    case ViewKind::kOverview: paint_overview(p); break;
    case ViewKind::kGallery: paint_gallery(p); break;
    case ViewKind::kBitmap: case ViewKind::kShip: case ViewKind::kBody: paint_picture(p); break;
    case ViewKind::kIcons: paint_icons(p); break;
    case ViewKind::kWave: paint_wave(p); break;
    case ViewKind::kPalette: paint_palette(p); break;
    default: text_block(p, kMargin, 12, g.view.text, kBad); break;
    }
    if (p.src_old) SelectObject(p.src, p.src_old);
    DeleteDC(p.src);

    SetViewportOrgEx(mem, 0, 0, nullptr);
    BitBlt(dc, 0, 0, w, hh, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old_font);
    SelectObject(mem, old_buf);
    DeleteObject(buf);
    DeleteDC(mem);
    EndPaint(h, &ps);

    const int ew = p.ext_w + kMargin, eh = p.ext_h + kMargin;
    if (ew != g.ext_w || eh != g.ext_h) {
        g.ext_w = ew;
        g.ext_h = eh;
        set_scroll();
    }
}

LRESULT CALLBACK canvas_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_PAINT:
        paint_canvas(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        set_scroll();
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_VSCROLL:
        on_scroll(SB_VERT, LOWORD(w));
        return 0;
    case WM_HSCROLL:
        on_scroll(SB_HORZ, LOWORD(w));
        return 0;
    case WM_MOUSEWHEEL: {
        const int lines = -GET_WHEEL_DELTA_WPARAM(w) * 3 / WHEEL_DELTA;
        const int bar = (GET_KEYSTATE_WPARAM(w) & MK_SHIFT) ? SB_HORZ : SB_VERT;
        scroll_to(bar, (bar == SB_VERT ? g.scroll_y : g.scroll_x) + lines * g.line_h);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetFocus(h);
        const int x = static_cast<short>(LOWORD(l)) + g.scroll_x;
        const int y = static_cast<short>(HIWORD(l)) + g.scroll_y;
        for (const auto& hit : g.hits)
            if (x >= hit.first.left && x < hit.first.right && y >= hit.first.top && y < hit.first.bottom) {
                select_node(hit.second);
                break;
            }
        return 0;
    }
    }
    return DefWindowProcW(h, msg, w, l);
}

}  // namespace
