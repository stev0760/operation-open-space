// isview part 5 of 6: the two owner-drawn list boxes. The tree is a LISTBOX whose rows are
// the expanded nodes, indented by level (comctl32 is not linked); the report is a LISTBOX of
// tab-separated rows drawn in pixel columns. Included once, by main.cpp.

namespace {

// ---- the tree

bool shown(int n) {
    for (int p = g.nodes[n].parent; p >= 0; p = g.nodes[p].parent)
        if (!g.nodes[p].expanded) return false;
    return true;
}

int node_row(int n) {
    const auto it = std::find(g.visible.begin(), g.visible.end(), n);
    return it == g.visible.end() ? -1 : static_cast<int>(it - g.visible.begin());
}

int current_node() {
    const LRESULT row = SendMessageW(g.tree, LB_GETCURSEL, 0, 0);
    return row >= 0 && row < static_cast<LRESULT>(g.visible.size()) ? g.visible[row] : -1;
}

// Rebuilds the rows after a node opened or closed, keeping the top row and selecting `keep`.
void refill_tree(int keep) {
    const LRESULT top = SendMessageW(g.tree, LB_GETTOPINDEX, 0, 0);
    int top_node = top >= 0 && top < static_cast<LRESULT>(g.visible.size()) ? g.visible[top] : -1;
    SendMessageW(g.tree, WM_SETREDRAW, FALSE, 0);
    SendMessageW(g.tree, LB_RESETCONTENT, 0, 0);
    g.visible.clear();
    for (int i = 0; i < static_cast<int>(g.nodes.size()); ++i)
        if (shown(i)) {
            g.visible.push_back(i);
            SendMessageW(g.tree, LB_ADDSTRING, 0, static_cast<LPARAM>(i));
        }
    while (top_node >= 0 && !shown(top_node)) top_node = g.nodes[top_node].parent;
    if (top_node >= 0) SendMessageW(g.tree, LB_SETTOPINDEX, static_cast<WPARAM>(node_row(top_node)), 0);
    const int sel = keep >= 0 ? node_row(keep) : -1;
    SendMessageW(g.tree, LB_SETCURSEL, static_cast<WPARAM>(sel), 0);
    SendMessageW(g.tree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g.tree, nullptr, TRUE);
}

void fill_rect(HDC dc, const RECT& r, COLORREF c) {
    SetBkColor(dc, c);
    ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &r, nullptr, 0, nullptr);
}

int text_width(HDC dc, const std::wstring& s) {
    SIZE sz{};
    GetTextExtentPoint32W(dc, s.c_str(), static_cast<int>(s.size()), &sz);
    return sz.cx;
}

void draw_tree_item(const DRAWITEMSTRUCT& d) {
    if (d.itemID == static_cast<UINT>(-1) || d.itemData >= g.nodes.size()) return;
    const Node& n = g.nodes[d.itemData];
    HDC dc = d.hDC;
    const RECT& rc = d.rcItem;
    fill_rect(dc, rc, (d.itemState & ODS_SELECTED) ? kSelBg : kPanel);
    SetBkMode(dc, TRANSPARENT);
    int x = rc.left + n.level * kIndent;
    const int mid = (rc.top + rc.bottom) / 2;
    if (n.has_children) {
        const int bx = x + kGlyphLeft;
        RECT box{bx, mid - 4, bx + 9, mid + 5};
        FrameRect(dc, &box, g.dim_brush);
        RECT h{bx + 2, mid, bx + 7, mid + 1};
        fill_rect(dc, h, kText);
        if (!n.expanded) {
            RECT v{bx + 4, mid - 2, bx + 5, mid + 3};
            fill_rect(dc, v, kText);
        }
    }
    x += kGlyphLeft + kGlyphWidth;
    const HGDIOBJ old = SelectObject(dc, n.level == 0 ? g.bold : g.font);
    SetTextColor(dc, kText);
    RECT tr{x, rc.top, rc.right - 4, rc.bottom};
    DrawTextW(dc, n.label.c_str(), static_cast<int>(n.label.size()), &tr,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    x += text_width(dc, n.label) + 10;
    SelectObject(dc, g.font);
    if (!n.note.empty() && x < rc.right - 8) {
        SetTextColor(dc, kDim);
        RECT nr{x, rc.top, rc.right - 4, rc.bottom};
        DrawTextW(dc, n.note.c_str(), static_cast<int>(n.note.size()), &nr,
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    SelectObject(dc, old);
    if (d.itemState & ODS_FOCUS) DrawFocusRect(dc, &rc);
}

// ---- the report

int col_x(const Column& c) { return 6 + c.chars * (g.view.mono ? g.mono_w : g.char_w); }

std::vector<std::wstring> split_tabs(const std::wstring& s) {
    std::vector<std::wstring> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t tab = s.find(L'\t', start);
        out.push_back(s.substr(start, tab - start));
        if (tab == std::wstring::npos) return out;
        start = tab + 1;
    }
}

// The left edge of column i: the view's column, or 16 characters per column without one.
int column_left(std::size_t i) {
    if (i < g.view.columns.size()) return col_x(g.view.columns[i]);
    return 6 + static_cast<int>(i) * 16 * g.char_w;
}

void set_lines(const std::vector<Line>& lines) {
    g.lines = lines;
    SendMessageW(g.report, WM_SETREDRAW, FALSE, 0);
    SendMessageW(g.report, LB_RESETCONTENT, 0, 0);
    SendMessageW(g.report, LB_INITSTORAGE, g.lines.size(), 0);
    for (std::size_t i = 0; i < g.lines.size(); ++i)
        SendMessageW(g.report, LB_ADDSTRING, 0, static_cast<LPARAM>(i));
    HDC dc = GetDC(g.report);
    const HGDIOBJ old = SelectObject(dc, g.view.mono ? g.mono : g.font);
    int extent = 0;
    for (const Line& l : g.lines) {
        const auto cols = split_tabs(l.text);
        extent = std::max(extent, column_left(cols.size() - 1) + text_width(dc, cols.back()) + 12);
    }
    SelectObject(dc, old);
    ReleaseDC(g.report, dc);
    SendMessageW(g.report, LB_SETHORIZONTALEXTENT, static_cast<WPARAM>(extent), 0);
    SendMessageW(g.report, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g.report, nullptr, TRUE);
}

void draw_report_item(const DRAWITEMSTRUCT& d) {
    if (d.itemID == static_cast<UINT>(-1) || d.itemData >= g.lines.size()) return;
    const Line& l = g.lines[d.itemData];
    HDC dc = d.hDC;
    const RECT& rc = d.rcItem;
    fill_rect(dc, rc, (d.itemState & ODS_SELECTED) ? kSelBg : kPanel);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, l.color);
    const HGDIOBJ old = SelectObject(dc, g.view.mono ? g.mono : g.font);
    const auto cols = split_tabs(l.text);
    for (std::size_t i = 0; i < cols.size(); ++i) {
        const bool last = i + 1 == cols.size();
        RECT r{rc.left + column_left(i), rc.top,
               last ? rc.left + 0x7FFF : rc.left + column_left(i + 1) - 6, rc.bottom};
        DrawTextW(dc, cols[i].c_str(), static_cast<int>(cols[i].size()), &r,
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | (last ? DT_NOCLIP : DT_END_ELLIPSIS));
    }
    SelectObject(dc, old);
    if (d.itemState & ODS_FOCUS) DrawFocusRect(dc, &rc);
}

// Copies the selected report rows (or every row) to the clipboard, tab-separated.
void copy_report() {
    const LRESULT count = SendMessageW(g.report, LB_GETSELCOUNT, 0, 0);
    std::vector<int> rows;
    if (count > 0) {
        rows.resize(static_cast<std::size_t>(count));
        SendMessageW(g.report, LB_GETSELITEMS, static_cast<WPARAM>(count), reinterpret_cast<LPARAM>(rows.data()));
    } else {
        for (int i = 0; i < static_cast<int>(g.lines.size()); ++i) rows.push_back(i);
    }
    std::wstring text;
    for (int r : rows)
        if (r >= 0 && r < static_cast<int>(g.lines.size())) text += g.lines[r].text + L"\r\n";
    if (!OpenClipboard(g.main)) return;
    EmptyClipboard();
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t))) {
        if (void* p = GlobalLock(h)) {
            std::memcpy(p, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(h);
            if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
        } else {
            GlobalFree(h);
        }
    }
    CloseClipboard();
    set_status(L"Copied " + num(static_cast<long long>(rows.size())) + L" rows to the clipboard");
}

}  // namespace
