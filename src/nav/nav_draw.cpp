#include "nav/nav_draw.h"

#include <cstdlib>

#include "assets/asset_store.h"
#include "intro/texts.h"

namespace is::nav {

namespace {

int digit_at(const std::string& s, std::size_t from_end) {
    return s.size() >= from_end ? s[s.size() - from_end] - '0' : 0;
}

void cut(std::string& s, int n) {
    for (int i = 0; i < n && !s.empty(); ++i) s.pop_back();
}

Op line(std::uint32_t pen, int x0, int y0, int x1, int y1) {
    Op o;
    o.kind = Op::Kind::kLine;
    o.colour = pen;
    o.x0 = x0, o.y0 = y0, o.x1 = x1, o.y1 = y1;
    return o;
}
Op pixel(int x, int y, std::uint32_t c) {
    Op o;
    o.kind = Op::Kind::kPixel;
    o.x0 = x, o.y0 = y;
    o.colour = c;
    return o;
}
Op fill(int x, int y, int w, int h, std::uint32_t c) {
    Op o;
    o.kind = Op::Kind::kFill;
    o.x0 = x, o.y0 = y, o.x1 = w, o.y1 = h;
    o.colour = c;
    return o;
}
Op text(const char* face, int height, int width, int weight, std::uint32_t colour, unsigned align,
        bool opaque, int x, int y, const std::string& s) {
    Op o;
    o.kind = Op::Kind::kText;
    o.face = face;
    o.height = height, o.width = width, o.weight = weight;
    o.colour = colour;
    o.align = align;
    o.opaque = opaque;
    o.bk = 0;
    o.x0 = x, o.y0 = y;
    o.text = s;
    return o;
}

}  // namespace

// ---- rows

TreeRowLook tree_row(const std::string& item, int index, int count, const std::string& next,
                     int left, int top, int bottom) {
    TreeRowLook r;
    std::string t = item;  // LB_GETTEXT into an 80-byte buffer [BYTES seg6:0x49c0]
    r.depth = digit_at(t, 3);
    r.indent = r.depth * 16 - 6;
    int x = 0;
    if (index == 0) {  // the leading space goes, I = 0 [BYTES seg6:0x4a4c]
        r.indent = 0;
        if (!t.empty()) t.erase(0, 1);
    }
    r.sub = digit_at(t, 2);
    r.state = digit_at(t, 1);
    cut(t, 3);
    // sub 1 -> 15, 0 and 2 -> 0; state 1, 2, 3 add 30, 60, 105 [BYTES seg6:0x4aed seg6:0x4bc9
    // seg6:0x4bd0]. Other sub values keep the start value, 0 [DESIGN].
    if (r.sub == 1) x = 15;
    if (r.state == 1) x += 30;
    else if (r.state == 2) x += 60;
    else if (r.state == 3) x += 105;
    r.glyph_x = x;
    r.highlight_left = left + r.indent;
    r.glyph_left = left + r.indent + 1;
    r.glyph_top = top + 1;
    r.text_x = left + r.indent + 18;
    r.text_y = top;
    r.text = intro::case_rule(t, 0);  // seg11:0x3fd4 mode 0 [BYTES seg6:0x4d8c]

    if (index != 0) {  // [BYTES seg6:0x4ba5-0x4cde]
        bool short_depth = false, last = false;
        if (static_cast<unsigned>(index) < static_cast<unsigned>(count - 1)) {
            if (r.depth > 1 && digit_at(next, 3) != r.depth) short_depth = true;
        } else {
            short_depth = last = true;
        }
        r.lines.push_back({left + 4, top, left + 4, last ? top + 6 : bottom});
        if (r.depth != 0) {
            const int lx = left + r.depth * 14 - 10;
            r.lines.push_back({lx, top, lx, short_depth ? top + 6 : bottom});
            r.lines.push_back({lx, top + 6, left + r.depth * 14 - 5, top + 6});
        }
    }
    return r;
}

int focus_indent(const std::string& item, int index) {
    return index == 0 ? 0 : digit_at(item, 3) * 16 - 6;
}

FileRowLook file_row(const std::string& item, int left, int top) {
    FileRowLook r;
    std::string t = item;  // seg6:0x5cbc [BYTES seg6:0x5cbc]
    r.digit = digit_at(t, 1);
    cut(t, 2);
    if (!t.empty() && t[0] == ' ') t.erase(0, 1);
    if (r.digit == 1) r.glyph_colour = 0x00FF0000u;
    else if (r.digit == 2) r.glyph_colour = 0x000000FFu;
    r.glyph_left = left + 1;
    r.glyph_top = top + 1;
    r.text_x = left + 15;
    r.text_y = top;
    r.text = intro::case_rule(t, 0);  // [BYTES seg6:0x4827]
    return r;
}

// ---- the bevel, the labels and the status band

std::vector<Op> bevel(int p, int l, int t, int r, int b) {
    std::vector<Op> v;
    const bool filled = p == -1;
    if (filled) p = 2;
    constexpr std::uint32_t kShade = 0x02808080, kLight = 0x02C0C0C0;
    constexpr std::uint32_t kBlackPen = 0x00000000, kWhitePen = 0x00FFFFFF;
    v.push_back(line(kShade, l, b, r, b));
    v.push_back(line(kShade, l + 1, b - 1, r - 1, b - 1));
    v.push_back(line(kShade, r - 1, b - 1, r - 1, t));
    v.push_back(line(kShade, l + p + 2, b - p - 3, l + p + 2, t + p + 2));
    v.push_back(line(kShade, l + p + 2, t + p + 2, r - p - 2, t + p + 2));
    if (p == 4) v.push_back(line(kShade, r, b, r, t));
    if (p != 4) v.push_back(line(kBlackPen, r, b, r, t));  // GetStockObject(BLACK_PEN)
    v.push_back(line(kWhitePen, l + p + 2, b - p - 2, r - p - 2, b - p - 2));  // WHITE_PEN
    v.push_back(line(kWhitePen, r - p - 2, b - p - 2, r - p - 2, t + p + 1));
    v.push_back(line(kWhitePen, l, b, l, t));
    v.push_back(line(kWhitePen, l, t, r, t));
    v.push_back(line(kWhitePen, l + 1, b - 1, l + 1, t + 1));
    v.push_back(line(kWhitePen, l + 1, t + 1, r - 1, t + 1));
    for (int i = 0; i < p; ++i) {  // `p` nested Rectangles, NULL_BRUSH or LTGRAY_BRUSH
        Op o;
        o.kind = Op::Kind::kRect;
        o.colour = kLight;
        o.filled = filled;
        o.x0 = l + i + 2, o.y0 = t + i + 2, o.x1 = r - i - 1, o.y1 = b - i - 1;
        v.push_back(o);
    }
    v.push_back(pixel(r, t, kShade));
    v.push_back(pixel(r - 1, t + 1, kLight));
    v.push_back(pixel(l, b, kLight));
    v.push_back(pixel(l + 1, b - 1, kLight));
    return v;
}

std::vector<Op> label(int x, int y, int height, int width, unsigned align, const std::string& s) {
    std::vector<Op> v;
    const bool thirteen = height == 13;
    const int h = thirteen ? 14 : height;
    if (h > 13 && !thirteen) v.push_back(text("Arial", h, width, 600, 0x02FFFFFF, align, false, x + 2, y + 2, s));
    v.push_back(text("Arial", h, width, 600, 0x025A5A5A, align, false, x, y, s));
    return v;
}

std::vector<Op> status_band(int cw, int ch, const std::string& raw_status) {
    std::vector<Op> v = bevel(-1, 0, ch - 25, cw, ch);
    for (Op& o : label(5, ch - 20, 14, 0, kTaLeft, status_display(raw_status))) v.push_back(std::move(o));
    return v;
}

std::vector<Op> preview_frame(int cw) {
    std::vector<Op> v = bevel(4, cw - 96, 85, cw - 24, 157);
    v.push_back(fill(cw - 92, 89, 64, 64, 0));
    return v;
}

Rect preview_rect(int cw) { return {cw - 92, 89, 64, 64}; }

// ---- seg9:0x352

std::vector<Op> placeholder(int x, int flag, int size, int y, const std::string& name_in,
                            int drive_offset) {
    std::string name = name_in;  // lstrcpy into a 110-byte buffer [BYTES seg9:0x36c]
    if (name.size() > 4 && name[name.size() - 4] == '.') name.resize(name.size() - 4);
    std::vector<Op> v;
    v.push_back(fill(x, y, size, size, 0));  // PatBlt BLACKNESS [BYTES seg9:0x3c2]
    constexpr std::uint32_t kWhite = 0x00FFFFFF, kBlack = 0, kGray = 0x00808080, kLight = 0x00C0C0C0;
    const int r = x + size - 1;
    if (flag != 0) {  // [BYTES seg9:0x3cf-0x5cb]
        v.push_back(fill(x, y, 32, 21, kGray));         // GRAY_BRUSH
        v.push_back(fill(x, y, 32, 9, 0x00800000));     // CreateSolidBrush(0x800000)
        v.push_back(fill(x, y, 8, 8, kLight));          // LTGRAY_BRUSH
        v.push_back(line(kWhite, x, y + 20, x, y));     // WHITE_PEN
        v.push_back(line(kWhite, x, y, x + 32, y));
        v.push_back(line(kLight, x, y + 20, r, y + 20));  // CreatePen(0, 1, 0xC0C0C0)
        v.push_back(line(kLight, r, y + 20, r, y));
        v.push_back(line(kBlack, x + 1, y + 8, x + 8, y + 8));  // BLACK_PEN
        v.push_back(line(kBlack, x + 8, y + 8, x + 8, y + 1));
        v.push_back(line(kBlack, x + 1, y + 5, x + 7, y + 5));
        v.push_back(line(kGray, x + 1, y + 3, x + 7, y + 3));  // CreatePen(0, 1, 0x808080)
        v.push_back(pixel(x + 1, y + 4, kBlack));
        v.push_back(pixel(x + 6, y + 4, kBlack));
    } else {  // [BYTES seg9:0x5ce-0x7dc]
        v.push_back(line(kWhite, x, y + 20, x, y));  // WHITE_PEN
        v.push_back(line(kWhite, x, y, x + 32, y));
        v.push_back(line(kGray, x, y + 20, r, y + 20));  // CreatePen(0, 1, 0x808080)
        v.push_back(line(kGray, r, y + 20, r, y));
        v.push_back(pixel(x, y, kBlack));
        v.push_back(pixel(x, y + 20, kBlack));
        v.push_back(pixel(r, y, kBlack));
        v.push_back(pixel(r, y + 20, kBlack));
        v.push_back(pixel(x + 1, y + 1, kLight));
        v.push_back(pixel(x + 1, y + 0x13, kLight));
        v.push_back(pixel(x + size - 2, y + 1, kLight));
        v.push_back(pixel(x + size - 2, y + 0x13, kLight));
        // "C:>" (DGROUP:0x72d) with byte 0 + DGROUP:0x94ac, Arial 10, OPAQUE on 0, TA_LEFT, green
        // [BYTES seg9:0x70e-0x7a6]
        std::string prompt = "C:>";
        prompt[0] = static_cast<char>(prompt[0] + drive_offset);
        v.push_back(text("Arial", 10, 0, 0, 0x0000FF00, kTaLeft, true, x + 2, y + 2, prompt));
    }
    // "Small Fonts" 7, background 0, TA_CENTER, white [BYTES seg9:0x7e1-0x861]. Flag 0 left the
    // mode OPAQUE; the other path leaves the DC's, OPAQUE by default [DESIGN].
    v.push_back(text("Small Fonts", 7, 0, 0, 0x00FFFFFF, kTaCenter, true, x + 16, y + 26, name));
    v.push_back(fill(x - y, y, y, size, 0));     // [BYTES seg9:0x8b1]
    v.push_back(fill(x + size, y, y, size, 0));  // [BYTES seg9:0x8b6]
    return v;
}

// ---- the preview's pixels

std::optional<fmt::IconCell> group_cell(const assets::IconGroup& group) {
    const fmt::Icon* best = nullptr;
    int best_d = 0, best_bpp = 0;
    for (const fmt::Icon& ic : group.images) {
        const int w = ic.image.width, h = ic.image.height;
        if (w <= 0 || h <= 0) continue;
        const int d = std::abs(w - fmt::kIconCell) + std::abs(h - fmt::kIconCell);
        const int bpp = ic.image.bpp;
        const auto better_bpp = [](int a, int b) {  // a better than b for an 8 bpp display
            if (a <= 8 && b <= 8) return a > b;
            if (a <= 8) return true;
            if (b <= 8) return false;
            return a < b;
        };
        if (!best || d < best_d || (d == best_d && better_bpp(bpp, best_bpp))) {
            best = &ic;
            best_d = d;
            best_bpp = bpp;
        }
    }
    if (!best) return std::nullopt;
    fmt::IconCell c;
    const int w = best->image.width, h = best->image.height;
    for (int y = 0; y < fmt::kIconCell; ++y)
        for (int x = 0; x < fmt::kIconCell; ++x) {
            // DrawIcon over black: (0 AND mask) XOR image is the image's colour everywhere
            const int sx = x * w / fmt::kIconCell, sy = y * h / fmt::kIconCell;
            c.bgra[static_cast<std::size_t>(y) * fmt::kIconCell + x] =
                0xFF000000u | (best->image.rgb_at(sx, sy) & 0xFFFFFFu);
        }
    return c;
}

// ---- IconReader

IconReader::IconReader(Load load) : load_(std::move(load)) {}

fmt::IconSource& IconReader::get(const std::string& path) {
    const std::string key = upper(path);
    for (auto it = cache_.begin(); it != cache_.end(); ++it)
        if (it->first == key) {
            cache_.splice(cache_.begin(), cache_, it);
            return cache_.front().second;
        }
    ++loads_;
    // A load that throws (out of memory, a filesystem error) is a file ExtractIcon cannot read:
    // count 0 (M3 s1 fixer) [DESIGN ADR 0012 decision 5].
    fmt::IconSource src(std::vector<std::uint8_t>{});
    try {
        if (load_) src = load_(path);
    } catch (const std::exception&) {
    }
    cache_.emplace_front(key, std::move(src));
    constexpr std::size_t kKeep = 8;
    while (cache_.size() > kKeep) cache_.pop_back();
    return cache_.front().second;
}

// No exception leaves the reader: a failure is ExtractIcon's 0 (M3 s1 fixer).
int IconReader::count(const std::string& path) {
    try {
        return get(path).count();
    } catch (const std::exception&) {
        return 0;
    }
}

bool IconReader::has_icon(const std::string& path) { return cell(path).has_value(); }

std::optional<fmt::IconCell> IconReader::cell(const std::string& path) {
    try {
        return get(path).cell(0);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace is::nav
