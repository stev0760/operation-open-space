// isview part 4 of 6: the views. Each builder fills a View from the asset store (or, for
// resources the store has no typed accessor for, from the module's bytes through the readers):
// a title, report rows, canvas pictures and what the toolbar button acts on. A builder may
// throw; show_node() then shows the error instead. Included once, by main.cpp.

namespace {

const wchar_t* const kSep = L" \u203A ";

std::wstring res_title(const ModuleInfo& m, const NeResource& r) {
    return widen(m.name) + kSep + widen(type_label(r)) + kSep + widen(id_label(r));
}

std::wstring quoted(const std::wstring& s) { return L"\u201C" + s + L"\u201D"; }

Line row(std::wstring text, COLORREF c = kText, int node = -1) { return Line{std::move(text), c, node}; }

int auto_zoom(int w, int h, int box_w, int box_h) {
    return std::clamp(std::min(box_w / std::max(1, w), box_h / std::max(1, h)), 1, kMaxZoom);
}

// A viewer default: a sprite whose width is a whole number of squares is taken as a strip of
// that many frames [DESIGN]. Some single pictures have widths that are multiples of their
// height (docs/re/resources.md), so the guess draws a grid on them; the Frames field
// overrides it. How the game slices frames is [OPEN].
int guess_frames(const Dib& d) {
    if (d.height <= 0 || d.width % d.height != 0) return 1;
    const int n = d.width / d.height;
    return n > 1 && n <= kMaxFrames ? n : 1;
}

void set_picture(View& v, Dib d, int transparent, bool sprite) {
    v.img = dib_image(d, transparent, Backdrop::kChecker);
    v.transparent = transparent;
    v.zoom = auto_zoom(d.width, d.height, 512, 300);
    v.frames = sprite ? guess_frames(d) : 1;
    v.dib = std::move(d);
}

std::wstring dib_text(const Dib& d) {
    return num(d.width) + L" \u00D7 " + num(d.height) + L" pixels, " + num(d.bpp) + L" bpp, " +
           num(static_cast<long long>(d.palette.size())) + L" palette entries, stored " +
           (d.bottom_up ? L"bottom-up" : L"top-down");
}

// ---- overview and identification

void view_overview(View& v) {
    v.kind = ViewKind::kOverview;
    v.title = L"Overview";
    v.subtitle = L"v0's page: the main window title, the Transformer and every sprite of IGRAPHIC.DLL";
}

// A file's identification result and its colour, for the identification and module views.
std::pair<std::wstring, COLORREF> ident_result(const isa::FileReport& f) {
    if (!f.error.empty()) return {L"unreadable: " + message(f.error), kBad};
    if (f.known) return {L"known", kGood};
    if (f.name_known) return {L"unknown: edited known file", kAccent};
    return {L"unknown", kBad};
}

void view_identify(View& v) {
    v.title = L"Identification against the table of known files (ADR 0002, decision 4)";
    if (!g.ident) {
        v.kind = ViewKind::kMessage;
        v.text = {L"identify() failed: " + message(g.ident_error)};
        return;
    }
    v.kind = ViewKind::kIdentify;
    const isa::Identification& id = *g.ident;
    std::wstring packages;
    for (isa::Package p : id.packages)
        packages += (packages.empty() ? L"" : L", ") + widen(isa::package_label(p));
    const std::size_t unknown = id.unknown_count();
    v.subtitle = std::wstring(id.form == isa::DirForm::kDistribution ? L"Distribution form" : L"Installed form") +
                 L" \u00B7 " + num(static_cast<long long>(id.files.size())) + L" files, " +
                 num(static_cast<long long>(id.files.size() - unknown)) + L" known, " +
                 num(static_cast<long long>(unknown)) + L" unknown" +
                 (packages.empty() ? L"" : L" \u00B7 " + packages) +
                 (g.version.empty() ? L"" : L" \u00B7 INSPACE.EXE FileVersion " + widen(g.version)) +
                 L" \u00B7 " + (isa::holds_game(g.dir) ? L"a game directory" : L"not a game directory");
    v.columns = {{L"File", 0}, {L"Stored as", 16}, {L"Size", 31}, {L"Result", 39},
                 {L"Package", 66}, {L"SHA-256 of the installed bytes", 84}};
    for (const isa::FileReport& f : id.files) {
        const auto [result, c] = ident_result(f);
        const std::wstring stored = f.path.filename() == f.name ? L"" : f.path.filename().wstring();
        v.lines.push_back(row(f.name.wstring() + L"\t" + stored + L"\t" +
                                  num(static_cast<long long>(f.size)) + L"\t" + result + L"\t" +
                                  (f.known ? widen(isa::package_label(f.known->package)) : L"") +
                                  L"\t" + widen(f.sha256),
                              c));
    }
    for (const fs::path& p : id.shadowed)
        v.lines.push_back(row(p.filename().wstring() + L"\t\t\tshadowed: another file has the same installed name",
                              kBad));
}

// ---- modules and types

const isa::FileReport* file_report(const fs::path& path) {
    if (!g.ident) return nullptr;
    for (const isa::FileReport& f : g.ident->files)
        if (f.path == path) return &f;
    return nullptr;
}

void view_module(View& v, const ModuleInfo& m, int mnode) {
    v.kind = ViewKind::kModule;
    v.title = widen(m.name) + (m.ne ? L" \u2014 module " + widen(m.ne->module_name()) : L"");
    v.subtitle = m.description.empty() ? m.path.wstring() : quoted(widen(m.description));
    v.columns = {{L"Field", 0}, {L"Value", 24}};
    v.lines.push_back(row(L"File\t" + widen(m.name)));
    v.lines.push_back(row(L"Stored as\t" + m.path.filename().wstring() +
                          (m.distribution_name ? L" (distribution name)" : L"")));
    v.lines.push_back(row(L"Path\t" + m.path.wstring()));
    v.lines.push_back(row(L"Installed size\t" + num(static_cast<long long>(m.size)) + L" bytes"));
    if (const isa::FileReport* f = file_report(m.path)) {
        const auto [result, c] = ident_result(*f);
        v.lines.push_back(row(L"Identification\t" + result +
                                  (f->known ? L", " + widen(isa::package_label(f->known->package)) : L""),
                              c));
        v.lines.push_back(row(L"SHA-256\t" + widen(f->sha256)));
    }
    if (!m.ne) {
        v.lines.push_back(row(L"Error\t" + message(m.error), kBad));
        return;
    }
    v.lines.push_back(row(L"Module name\t" + widen(m.ne->module_name())));
    v.lines.push_back(row(L"Description\t" + widen(m.description)));
    v.lines.push_back(row(L"Resource alignment\tshift " + num(m.ne->align_shift()) + L" (" +
                          num(1LL << m.ne->align_shift()) + L"-byte units)"));
    v.lines.push_back(row(L"Resources\t" + num(static_cast<long long>(m.ne->resources().size()))));
    v.lines.push_back(row(L"Strings\t" + num(static_cast<long long>(string_count(*m.ne)))));
    v.lines.push_back(row(L""));
    v.lines.push_back(row(L"Resource type\tCount (double-click a row to open the type)", kDim));
    for (int i = 0; i < static_cast<int>(g.nodes.size()); ++i)
        if (g.nodes[i].parent == mnode)
            v.lines.push_back(row(g.nodes[i].label + L"\t" + g.nodes[i].note, kText, i));
}

void view_strings(View& v, const ModuleInfo& m, const NeResource* block) {
    v.kind = ViewKind::kStrings;
    v.columns = {{L"Id", 0}, {L"Text (cp1252; control characters escaped)", 8}};
    auto add = [&](unsigned id, const std::string& s) {
        v.lines.push_back(row(num(id) + L"\t" + escaped(widen(s))));
    };
    if (block) {
        v.title = res_title(m, *block) + L" (ids " + num((block->id - 1u) * 16u) + L"\u2013" +
                  num((block->id - 1u) * 16u + 15u) + L")";
        for (unsigned i = 0; i < 16 && block->id > 0; ++i) {
            const auto id = static_cast<std::uint16_t>((block->id - 1u) * 16u + i);
            if (auto s = load_string(*m.ne, id)) add(id, *s);
        }
    } else {
        v.title = widen(m.name) + kSep + L"STRING, every string";
        for (const auto& s : all_strings(*m.ne)) add(s.first, s.second);
    }
}

void view_gallery(View& v, int tnode) {
    const Node& t = g.nodes[tnode];
    const ModuleInfo& m = g.modules[t.module];
    v.kind = ViewKind::kGallery;
    v.title = widen(m.name) + kSep + t.label;
    for (int i = 0; i < static_cast<int>(g.nodes.size()); ++i) {
        if (g.nodes[i].parent != tnode) continue;
        const NeResource& r = *g.nodes[i].res;
        Tile tile;
        tile.node = i;
        tile.label = widen(id_label(r));
        try {
            const Dib d = parse_dib(m.ne->data(r), m.ne->size(r));
            const bool sprite = int_type(r) == isa::kSpriteType;
            tile.img = dib_image(d, sprite ? sprite_transparent(d) : -1, Backdrop::kBackground);
        } catch (const std::exception&) {
            tile.label += L" (?)";
        }
        v.tiles.push_back(std::move(tile));
    }
    v.subtitle = num(static_cast<long long>(v.tiles.size())) +
                 L" bitmaps, scaled to fit their cells; click one to open it";
}

void view_type(View& v, int tnode) {
    const Node& t = g.nodes[tnode];
    const ModuleInfo& m = g.modules[t.module];
    const std::uint16_t type = int_type(*t.res);
    if (type == RT16_STRING) {
        view_strings(v, m, nullptr);
        return;
    }
    if (type == isa::kSpriteType || type == RT16_BITMAP) {
        view_gallery(v, tnode);
        return;
    }
    v.kind = ViewKind::kTypeTable;
    v.title = widen(m.name) + kSep + t.label;
    v.subtitle = t.note + L" resources; double-click a row to open it";
    v.columns = {{L"Id", 0}, {L"Size", 14}, {L"File offset", 22}, {L"Flags", 34}, {L"Contents", 42}};
    for (int i = 0; i < static_cast<int>(g.nodes.size()); ++i) {
        if (g.nodes[i].parent != tnode) continue;
        const NeResource& r = *g.nodes[i].res;
        v.lines.push_back(row(widen(id_label(r)) + L"\t" + num(r.length) + L"\t" + hex(r.offset, 6) +
                                  L"\t" + hex(r.flags) + L"\t" + g.nodes[i].note,
                              kText, i));
    }
}

// ---- dialogs, menus, accelerators

std::wstring style_names(std::uint32_t s) {
    static const std::pair<std::uint32_t, const wchar_t*> k[] = {
        {WS_CAPTION, L"WS_CAPTION"}, {WS_POPUP, L"WS_POPUP"}, {WS_CHILD, L"WS_CHILD"},
        {WS_VISIBLE, L"WS_VISIBLE"}, {WS_DISABLED, L"WS_DISABLED"},
        {WS_CLIPSIBLINGS, L"WS_CLIPSIBLINGS"}, {WS_CLIPCHILDREN, L"WS_CLIPCHILDREN"},
        {WS_BORDER, L"WS_BORDER"}, {WS_DLGFRAME, L"WS_DLGFRAME"}, {WS_VSCROLL, L"WS_VSCROLL"},
        {WS_HSCROLL, L"WS_HSCROLL"}, {WS_SYSMENU, L"WS_SYSMENU"}, {WS_THICKFRAME, L"WS_THICKFRAME"},
        {WS_MINIMIZEBOX, L"WS_MINIMIZEBOX"}, {WS_MAXIMIZEBOX, L"WS_MAXIMIZEBOX"},
        {DS_ABSALIGN, L"DS_ABSALIGN"}, {DS_SYSMODAL, L"DS_SYSMODAL"}, {DS_LOCALEDIT, L"DS_LOCALEDIT"},
        {DS_SETFONT, L"DS_SETFONT"}, {DS_MODALFRAME, L"DS_MODALFRAME"},
        {DS_NOIDLEMSG, L"DS_NOIDLEMSG"}, {DS_SETFOREGROUND, L"DS_SETFOREGROUND"},
    };
    std::wstring o;
    for (const auto& e : k)
        if ((s & e.first) == e.first) {
            o += (o.empty() ? L"" : L"|") + std::wstring(e.second);
            s &= ~e.first;
        }
    if (s) o += (o.empty() ? L"" : L"|") + hex(s, 8);
    return o;
}

std::wstring control_class(const DialogItem16& it) {
    static const wchar_t* const k[] = {L"BUTTON", L"EDIT", L"STATIC", L"LISTBOX", L"SCROLLBAR", L"COMBOBOX"};
    if (it.class_atom >= 0x80 && it.class_atom <= 0x85) return k[it.class_atom - 0x80];
    if (it.class_atom) return hex(it.class_atom, 2);
    return widen(it.class_name);
}

std::wstring name_or_ord(const NameOrOrd& n) {
    return n.is_ord ? L"#" + num(n.ord) : escaped(widen(n.name));
}

void view_dialog(View& v, const ModuleInfo& m, const NeResource& r) {
    const Dialog16 d = parse_dialog16(m.ne->data(r), m.ne->size(r));
    // DS_NOFAILCREATE: a control whose class does not exist does not stop the viewer from
    // showing the rest [DESIGN].
    if (is_language_module(m) && !named_id(r))
        v.dialog_tmpl = g.store->dialog_template(r.id, {}, DS_NOFAILCREATE);
    else
        v.dialog_tmpl = dialog_to_win32(d, DS_NOFAILCREATE);
    v.kind = ViewKind::kDialog;
    v.title = res_title(m, r) + L" " + quoted(widen(d.caption));
    v.subtitle = hex(d.style, 8) + L" " + style_names(d.style) + L" \u00B7 at " + num(d.x) + L"," +
                 num(d.y) + L", " + num(d.cx) + L" \u00D7 " + num(d.cy) + L" dialog units";
    if (d.style & kDsSetFont) v.subtitle += L" \u00B7 " + widen(d.face) + L" " + num(d.point_size);
    if (!d.menu.empty()) v.subtitle += L" \u00B7 menu " + name_or_ord(d.menu);
    if (!d.class_name.empty()) v.subtitle += L" \u00B7 class " + widen(d.class_name);
    v.subtitle += L" \u00B7 " + num(static_cast<long long>(d.items.size())) + L" controls";
    v.columns = {{L"Id", 0}, {L"Class", 7}, {L"Text", 18}, {L"X", 50}, {L"Y", 55},
                 {L"Width", 60}, {L"Height", 66}, {L"Style", 74}};
    for (const DialogItem16& it : d.items)
        v.lines.push_back(row(num(it.id) + L"\t" + control_class(it) + L"\t" + name_or_ord(it.text) +
                              L"\t" + num(it.x) + L"\t" + num(it.y) + L"\t" + num(it.cx) + L"\t" +
                              num(it.cy) + L"\t" + hex(it.style, 8)));
}

bool is_separator(const MenuItem16& it) {
    return !(it.flags & kMfPopup) && ((it.flags & MF_SEPARATOR) || (it.id == 0 && it.text.empty()));
}

std::wstring menu_flags(std::uint16_t f) {
    static const std::pair<std::uint16_t, const wchar_t*> k[] = {
        {MF_GRAYED, L"GRAYED"}, {MF_DISABLED, L"DISABLED"}, {MF_BITMAP, L"BITMAP"},
        {MF_CHECKED, L"CHECKED"}, {kMfPopup, L"POPUP"}, {MF_MENUBARBREAK, L"MENUBARBREAK"},
        {MF_MENUBREAK, L"MENUBREAK"}, {kMfEnd, L"END"}, {MF_OWNERDRAW, L"OWNERDRAW"},
        {MF_SEPARATOR, L"SEPARATOR"}, {MF_HELP, L"HELP"},
    };
    std::wstring o = hex(f);
    for (const auto& e : k)
        if (f & e.first) o += L" " + std::wstring(e.second);
    return o;
}

void menu_lines(const std::vector<MenuItem16>& items, int depth, std::vector<Line>& out) {
    for (const MenuItem16& it : items) {
        const std::wstring indent(static_cast<std::size_t>(depth) * 4, L' ');
        const bool popup = it.flags & kMfPopup;
        const std::wstring text = is_separator(it) ? L"---- separator" : escaped(widen(it.text));
        out.push_back(row(indent + text + L"\t" + (popup ? L"popup" : num(it.id)) + L"\t" + menu_flags(it.flags),
                          popup ? kAccent : (is_separator(it) ? kDim : kText)));
        if (popup) menu_lines(it.children, depth + 1, out);
    }
}

void view_menu(View& v, const ModuleInfo& m, const NeResource& r) {
    v.menu = parse_menu16(m.ne->data(r), m.ne->size(r));
    v.kind = ViewKind::kMenu;
    v.title = res_title(m, r);
    v.subtitle = num(static_cast<long long>(v.menu.size())) +
                 L" top-level items; the button opens the menu as a popup (Enter or a double-click in the tree does too)";
    v.columns = {{L"Item", 0}, {L"Id", 40}, {L"Flags", 48}};
    menu_lines(v.menu, 0, v.lines);
}

std::wstring strip_menu_text(const std::string& s) {
    std::wstring o;
    for (wchar_t c : widen(s)) {
        if (c == L'\t') break;
        if (c != L'&') o += c;
    }
    return o;
}

void collect_commands(const std::vector<MenuItem16>& items, const std::wstring& prefix,
                      std::map<std::uint16_t, std::wstring>& out) {
    for (const MenuItem16& it : items) {
        const std::wstring t = prefix + strip_menu_text(it.text);
        if (it.flags & kMfPopup) collect_commands(it.children, t + kSep, out);
        else if (it.id && !out.count(it.id)) out[it.id] = t;
    }
}

// Command id -> "MENU 101: Game > New Game" over every MENU of the module.
std::map<std::uint16_t, std::wstring> menu_commands(const NeModule& m) {
    std::map<std::uint16_t, std::wstring> out;
    for (const NeResource* r : m.all(RT16_MENU)) {
        try {
            collect_commands(parse_menu16(m.data(*r), m.size(*r)),
                             L"MENU " + widen(id_label(*r)) + L": ", out);
        } catch (const std::exception&) {
        }
    }
    return out;
}

std::wstring key_name(const Accel16& a) {
    if (!(a.flags & FVIRTKEY)) {
        if (a.key > 0xFF) return hex(a.key);
        const std::wstring c = widen(std::string(1, static_cast<char>(a.key)));
        return printable(c[0]) ? L"\"" + c + L"\"" : hex(a.key);
    }
    const unsigned k = a.key;
    if (k >= 0x70 && k <= 0x87) return L"VK_F" + num(k - 0x6F);
    if ((k >= '0' && k <= '9') || (k >= 'A' && k <= 'Z')) return std::wstring(1, static_cast<wchar_t>(k));
    if (k >= 0x60 && k <= 0x69) return L"VK_NUMPAD" + num(k - 0x60);
    switch (k) {
    case VK_BACK: return L"VK_BACK";
    case VK_TAB: return L"VK_TAB";
    case VK_RETURN: return L"VK_RETURN";
    case VK_ESCAPE: return L"VK_ESCAPE";
    case VK_SPACE: return L"VK_SPACE";
    case VK_PRIOR: return L"VK_PRIOR";
    case VK_NEXT: return L"VK_NEXT";
    case VK_END: return L"VK_END";
    case VK_HOME: return L"VK_HOME";
    case VK_LEFT: return L"VK_LEFT";
    case VK_UP: return L"VK_UP";
    case VK_RIGHT: return L"VK_RIGHT";
    case VK_DOWN: return L"VK_DOWN";
    case VK_INSERT: return L"VK_INSERT";
    case VK_DELETE: return L"VK_DELETE";
    case VK_MULTIPLY: return L"VK_MULTIPLY";
    case VK_ADD: return L"VK_ADD";
    case VK_SEPARATOR: return L"VK_SEPARATOR";
    case VK_SUBTRACT: return L"VK_SUBTRACT";
    case VK_DECIMAL: return L"VK_DECIMAL";
    case VK_DIVIDE: return L"VK_DIVIDE";
    default: return hex(k, 2);
    }
}

std::wstring accel_modifiers(std::uint8_t f) {
    std::wstring o;
    if (f & FCONTROL) o += L"Ctrl+";
    if (f & FSHIFT) o += L"Shift+";
    if (f & FALT) o += L"Alt+";
    return o.empty() ? L"" : o.substr(0, o.size() - 1);
}

std::wstring accel_flags(std::uint8_t f) {
    std::wstring o = hex(f, 2);
    if (f & FVIRTKEY) o += L" VIRTKEY";
    if (f & FNOINVERT) o += L" NOINVERT";
    if (f & FSHIFT) o += L" SHIFT";
    if (f & FCONTROL) o += L" CONTROL";
    if (f & FALT) o += L" ALT";
    return o;
}

void view_accel(View& v, const ModuleInfo& m, const NeResource& r) {
    const auto entries = parse_accel16(m.ne->data(r), m.ne->size(r));
    const auto commands = menu_commands(*m.ne);
    v.kind = ViewKind::kAccel;
    v.title = res_title(m, r);
    v.subtitle = num(static_cast<long long>(entries.size())) +
                 L" entries; the menu item is the first of the module's menus with that command";
    v.columns = {{L"#", 0}, {L"Key", 4}, {L"Modifiers", 18}, {L"Command", 30},
                 {L"Menu item", 40}, {L"Flags", 90}};
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const Accel16& a = entries[i];
        const auto it = commands.find(a.cmd);
        v.lines.push_back(row(num(static_cast<long long>(i)) + L"\t" + key_name(a) + L"\t" +
                              accel_modifiers(a.flags) + L"\t" + num(a.cmd) + L"\t" +
                              (it == commands.end() ? L"" : it->second) + L"\t" + accel_flags(a.flags)));
    }
}

// ---- pictures, icons, sounds

void view_bitmap(View& v, const ModuleInfo& m, const NeResource& r) {
    const bool sprite = int_type(r) == isa::kSpriteType;
    Dib d;
    if (sprite && !named_id(r))
        d = g.store->sprite(r.id, m.name);
    else if (int_type(r) == RT16_BITMAP && named_id(r))
        d = g.store->bitmap(r.name, m.name);
    else
        d = parse_dib(m.ne->data(r), m.ne->size(r));
    v.kind = ViewKind::kBitmap;
    v.title = res_title(m, r);
    v.subtitle = sprite ? L"A sprite DIB (file-format, with BITMAPFILEHEADER)" : L"A packed DIB";
    v.text.push_back(dib_text(d));
    const int transparent = sprite ? sprite_transparent(d) : -1;
    if (transparent >= 0)
        v.text.push_back(L"Palette index " + num(transparent) +
                         L" is drawn transparent (checkered): a viewer choice, since how the game masks "
                         L"sprites is open (docs/re/resources.md)");
    set_picture(v, std::move(d), transparent, sprite);
}

void view_icons(View& v, const ModuleInfo& m, const NeResource& r) {
    v.kind = ViewKind::kIcons;
    v.title = res_title(m, r);
    if (int_type(r) == RT16_GROUP_ICON) {
        const isa::IconGroup gi = named_id(r) ? g.store->group_icon(m.name, r.name)
                                              : g.store->group_icon(m.name, r.id);
        v.subtitle = num(static_cast<long long>(gi.entries.size())) + L" images; as a .ico file, " +
                     num(static_cast<long long>(gi.ico.size())) +
                     L" bytes. Masked pixels are checkered; click an image to open its ICON.";
        for (std::size_t i = 0; i < gi.entries.size() && i < gi.images.size(); ++i) {
            const IconDirEntry& e = gi.entries[i];
            Tile t;
            t.img = icon_image(gi.images[i]);
            t.node = resource_node(m.ne->find(RT16_ICON, e.id));
            t.label = L"ICON " + num(e.id) + L": " + num(e.width ? e.width : 256) + L" \u00D7 " +
                      num(e.height ? e.height : 256) + L", " +
                      (e.colors ? num(e.colors) + L" colours" : std::wstring(L"256 or more colours")) +
                      L", " + num(e.planes) + L" plane, " + num(e.bpp) + L" bpp, " + num(e.bytes) +
                      L" bytes; image " + dims(gi.images[i].image);
            v.tiles.push_back(std::move(t));
        }
    } else {
        const Icon ic = parse_icon(m.ne->data(r), m.ne->size(r));
        v.subtitle = L"A single icon image. Masked pixels are checkered.";
        Tile t;
        t.img = icon_image(ic);
        t.label = dims(ic.image) + L", " + num(static_cast<long long>(ic.size)) + L" bytes";
        v.tiles.push_back(std::move(t));
    }
}

void view_wave(View& v, const ModuleInfo& m, const NeResource& r) {
    v.sound = g.store->wav(m.name, r.id);
    v.has_sound = true;
    v.kind = ViewKind::kWave;
    v.title = res_title(m, r);
    const Wave& w = v.sound.wave;
    v.subtitle = L"Play hands the RIFF file to PlaySoundW(SND_MEMORY | SND_ASYNC)";
    v.text.push_back(L"RIFF WAVE file of " + num(static_cast<long long>(w.file_size)) +
                     L" bytes; the resource holds " + num(r.length) + L" bytes with the NE padding");
    v.text.push_back(L"Format tag " + num(w.format_tag) + (w.format_tag == 1 ? L" (PCM)" : L"") + L", " +
                     num(w.channels) + (w.channels == 1 ? L" channel, " : L" channels, ") +
                     num(w.rate) + L" Hz, " + num(w.bits) + L" bits");
    v.text.push_back(L"Byte rate " + num(w.byte_rate) + L", block align " + num(w.block_align));
    v.text.push_back(L"Data chunk: " + num(static_cast<long long>(w.data_size)) + L" bytes at offset " +
                     num(static_cast<long long>(w.data_offset)) + L", " +
                     num(static_cast<long long>(w.frames())) + L" frames, " +
                     (w.rate ? num(static_cast<long long>((w.frames() * 1000 + w.rate / 2) / w.rate)) : L"?") + L" ms");
}

const wchar_t* file_os_name(std::uint32_t os) {
    switch (os) {
    case 0x00000001: return L"VOS__WINDOWS16";
    case 0x00010001: return L"VOS_DOS_WINDOWS16";
    case 0x00000004: return L"VOS__WINDOWS32";
    case 0x00040004: return L"VOS_NT_WINDOWS32";
    case 0x00010000: return L"VOS_DOS";
    default: return L"";
    }
}

const wchar_t* file_type_name(std::uint32_t t) {
    switch (t) {
    case 1: return L"VFT_APP";
    case 2: return L"VFT_DLL";
    case 3: return L"VFT_DRV";
    case 4: return L"VFT_FONT";
    case 5: return L"VFT_VXD";
    case 7: return L"VFT_STATIC_LIB";
    default: return L"VFT_UNKNOWN";
    }
}

void view_version(View& v, const ModuleInfo& m, const NeResource& r) {
    const VersionInfo vi = parse_version16(m.ne->data(r), m.ne->size(r));
    v.kind = ViewKind::kVersion;
    v.title = res_title(m, r);
    v.subtitle = L"VS_VERSION_INFO, " + num(static_cast<long long>(vi.size)) + L" bytes (Win16 layout, cp1252 text)";
    v.columns = {{L"Key", 0}, {L"Value", 26}};
    if (vi.fixed) {
        const FixedFileInfo& f = *vi.fixed;
        v.lines.push_back(row(L"VS_FIXEDFILEINFO", kAccent));
        v.lines.push_back(row(L"    FileVersion\t" + widen(f.file_version())));
        v.lines.push_back(row(L"    ProductVersion\t" + widen(f.product_version())));
        v.lines.push_back(row(L"    FileFlagsMask\t" + hex(f.file_flags_mask, 8)));
        v.lines.push_back(row(L"    FileFlags\t" + hex(f.file_flags, 8)));
        v.lines.push_back(row(L"    FileOS\t" + hex(f.file_os, 8) + L" " + file_os_name(f.file_os)));
        v.lines.push_back(row(L"    FileType\t" + hex(f.file_type, 8) + L" " + file_type_name(f.file_type)));
        v.lines.push_back(row(L"    FileSubtype\t" + hex(f.file_subtype, 8)));
        v.lines.push_back(row(L"    FileDate\t" + hex(f.file_date_ms, 8) + L" " + hex(f.file_date_ls, 8)));
    }
    for (const VersionStringTable& t : vi.string_tables) {
        v.lines.push_back(row(L"StringFileInfo " + widen(t.key), kAccent));
        for (const auto& s : t.strings)
            v.lines.push_back(row(L"    " + widen(s.first) + L"\t" + escaped(widen(s.second))));
    }
    if (!vi.vars.empty()) v.lines.push_back(row(L"VarFileInfo", kAccent));
    for (const auto& var : vi.vars) {
        std::wstring words;
        for (std::uint16_t w : var.second) words += hex(w) + L" ";
        v.lines.push_back(row(L"    " + widen(var.first) + L"\t" + words));
    }
}

// Text when text_length() accepts the bytes, else hex.
void view_data(View& v, const ModuleInfo& m, const NeResource& r) {
    const std::uint8_t* p = m.ne->data(r);
    const std::size_t n = m.ne->size(r);
    const std::size_t end = text_length(p, n);
    const bool text = end > 0;
    v.kind = ViewKind::kData;
    v.title = res_title(m, r);
    v.subtitle = num(static_cast<long long>(n)) + L" bytes in the resource table; shown as " +
                 (text ? L"cp1252 text, " + num(static_cast<long long>(end)) + L" bytes before the trailing zeros"
                       : std::wstring(L"hex"));
    if (text) {
        v.columns = {{L"Text", 0}};
        std::string line;
        for (std::size_t i = 0; i <= end; ++i) {
            if (i == end || p[i] == '\n') {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                v.lines.push_back(row(escaped(widen(line))));
                line.clear();
            } else {
                line.push_back(static_cast<char>(p[i]));
            }
        }
        return;
    }
    v.mono = true;
    v.columns = {{L"Offset", 0}, {L"Bytes", 8}, {L"cp1252", 58}};
    for (std::size_t off = 0; off < n; off += 16) {
        std::wstring bytes, chars;
        for (std::size_t i = off; i < off + 16 && i < n; ++i) {
            wchar_t b[4];
            std::swprintf(b, 4, L"%02X ", static_cast<unsigned>(p[i]));
            bytes += b;
            const std::wstring c = widen(std::string(1, static_cast<char>(p[i])));
            chars += printable(c[0]) ? c : L".";
        }
        v.lines.push_back(row(hex(static_cast<std::uint32_t>(off), 4).substr(2) + L"\t" + bytes + L"\t" + chars));
    }
}

void view_palette(View& v, const ModuleInfo& m, const NeResource& r) {
    v.palette = g.store->palette5500();
    v.kind = ViewKind::kPalette;
    v.title = res_title(m, r);
    v.subtitle = L"92 colours that the game copies into its LOGPALETTE entries 11-102 (src/formats/records.h)";
}

void view_resource(View& v, const ModuleInfo& m, const NeResource& r) {
    switch (int_type(r)) {
    case RT16_STRING: view_strings(v, m, &r); return;
    case RT16_DIALOG: view_dialog(v, m, r); return;
    case RT16_MENU: view_menu(v, m, r); return;
    case RT16_ACCELERATOR: view_accel(v, m, r); return;
    case RT16_BITMAP: view_bitmap(v, m, r); return;
    case RT16_ICON: case RT16_GROUP_ICON: view_icons(v, m, r); return;
    case RT16_VERSION: view_version(v, m, r); return;
    default: break;
    }
    if (int_type(r) == isa::kSpriteType) {
        view_bitmap(v, m, r);
        return;
    }
    if (int_type(r) == kWavResourceType && !named_id(r) && m.ne->size(r) >= 4 &&
        std::memcmp(m.ne->data(r), "RIFF", 4) == 0) {
        view_wave(v, m, r);
        return;
    }
    if (ascii_ieq(m.name, isa::kGameDll) && int_type(r) == Palette5500::kType && r.id == Palette5500::kId) {
        view_palette(v, m, r);
        return;
    }
    view_data(v, m, r);
}

// ---- ships

void need_ships() {
    if (!g.ships) throw isa::NotFound("MASTER.BOD and MASTER.SHP were not read: " + g.ships_error);
}

void view_ships(View& v) {
    v.title = L"MASTER.SHP: ships";
    need_ships();
    v.kind = ViewKind::kShips;
    const auto& recs = g.ships->ships.records;
    v.subtitle = num(static_cast<long long>(recs.size())) + L" records of 65 bytes, " +
                 num(static_cast<long long>(g.ships->ships.tail.size())) +
                 L" bytes after them; double-click a row to open it";
    v.columns = {{L"#", 0}, {L"Name", 5}, {L"Author", 21}, {L"Body", 37}, {L"Weapon", 58},
                 {L"Team", 84}, {L"Flags", 90}};
    for (std::size_t i = 0; i < recs.size(); ++i) {
        const ShipRecord& s = recs[i];
        v.lines.push_back(row(num(static_cast<long long>(i)) + L"\t" + widen(s.name) + L"\t" +
                                  widen(s.author) + L"\t" + num(s.body) + L" " + body_name(s.body) +
                                  L"\t" + weapon_label(s.weapon) + L"\t" + num(s.team) + L"\t" +
                                  ship_flags(s.flags),
                              s.name.empty() ? kDim : kText, ship_node(NodeKind::kShip, static_cast<int>(i))));
    }
}

void body_picture(View& v, const BodyRecord& b) {
    try {
        if (b.bitmap_id != kBodyBitmapFromFile) {
            Dib d = g.store->sprite(b.bitmap_id);
            v.text.push_back(L"Body bitmap: IGRAPHIC.DLL #4000 " + num(b.bitmap_id));
            v.text.push_back(dib_text(d));
            const int t = sprite_transparent(d);
            set_picture(v, std::move(d), t, true);
        } else {
            const std::string file = b.bitmap_file + ".DIB";
            const auto bytes = g.store->files().read(file);
            Dib d = parse_dib(bytes.data(), bytes.size());
            v.text.push_back(L"Body bitmap: the file " + widen(file));
            v.text.push_back(dib_text(d));
            const int t = sprite_transparent(d);
            set_picture(v, std::move(d), t, true);
        }
    } catch (const std::exception& x) {
        v.text.push_back(L"No body bitmap: " + message(x.what()));
    }
}

void body_lines(View& v, const BodyRecord& b, std::size_t index) {
    v.lines.push_back(row(L"\tMASTER.BOD record " + num(static_cast<long long>(index)) + L", file offset " +
                              hex(static_cast<std::uint32_t>(index * kBodyRecordSize), 4),
                          kAccent));
    v.lines.push_back(row(L"+0x00\tName\t" + widen(b.name)));
    v.lines.push_back(row(L"+0x15\tword_15\t" + num(b.word_15)));
    v.lines.push_back(row(L"+0x17\tword_17\t" + num(b.word_17)));
    v.lines.push_back(row(L"+0x19\tbyte_19\t" + num(b.byte_19)));
    v.lines.push_back(row(L"+0x1a\tbyte_1a\t" + num(b.byte_1a)));
    v.lines.push_back(row(L"+0x1b\tStrength base\t" + num(b.strength_base)));
    v.lines.push_back(row(L"+0x1c\tbyte_1c\t" + num(b.byte_1c)));
    v.lines.push_back(row(L"+0x1d\tbyte_1d\t" + num(b.byte_1d)));
    v.lines.push_back(row(L"+0x1e\tbyte_1e\t" + num(b.byte_1e)));
    v.lines.push_back(row(L"+0x1f\tbyte_1f\t" + num(b.byte_1f)));
    v.lines.push_back(row(L"+0x20\tFlags\t" + body_flags(b.flags)));
    v.lines.push_back(row(L"+0x21\tBitmap id\t" + (b.bitmap_id == kBodyBitmapFromFile
                                                      ? hex(b.bitmap_id) + L" (from the file)"
                                                      : num(b.bitmap_id) + L" (IGRAPHIC.DLL #4000)")));
    v.lines.push_back(row(L"+0x23\tBitmap file\t" + widen(b.bitmap_file)));
}

void view_ship(View& v, int index) {
    need_ships();
    const ShipRecord& s = g.ships->ships.records.at(static_cast<std::size_t>(index));
    v.kind = ViewKind::kShip;
    v.title = L"MASTER.SHP" + std::wstring(kSep) + num(index) + L" " + quoted(widen(s.name));
    v.subtitle = L"Every field of the ship record and of its body record (src/formats/ships.h)";
    v.columns = {{L"Offset", 0}, {L"Field", 7}, {L"Value", 21}};
    v.lines.push_back(row(L"\tMASTER.SHP record " + num(index) + L", file offset " +
                              hex(static_cast<std::uint32_t>(index) * kShipRecordSize, 4),
                          kAccent));
    v.lines.push_back(row(L"+0x00\tFuel tank\t" + num(s.fuel_tank)));
    v.lines.push_back(row(L"+0x01\tThruster\t" + num(s.thruster)));
    v.lines.push_back(row(L"+0x02\tArmor\t" + num(s.armor)));
    v.lines.push_back(row(L"+0x03\tTeam\t" + num(s.team) + L" (Team" + num(s.team + 1) + L" in [Teams])"));
    v.lines.push_back(row(L"+0x04\tTo friends\t" + num(s.to_friends) + L" " + to_friends_label(s.to_friends)));
    v.lines.push_back(row(L"+0x05\tTo enemies\t" + num(s.to_enemies) + L" " + to_enemies_label(s.to_enemies)));
    v.lines.push_back(row(L"+0x06\tWeapon\t" + weapon_label(s.weapon)));
    v.lines.push_back(row(L"+0x07\tbyte_07\t" + num(s.byte_07)));
    v.lines.push_back(row(L"+0x08\tbyte_08\t" + num(s.byte_08)));
    v.lines.push_back(row(L"+0x09\tFlags\t" + ship_flags(s.flags)));
    v.lines.push_back(row(L"+0x0a\tName\t" + widen(s.name)));
    v.lines.push_back(row(L"+0x1f\tHit sound\t" + (s.hit_sound == kShipSoundFromFile
                                                      ? hex(s.hit_sound) + L" (from the file)"
                                                      : num(s.hit_sound))));
    v.lines.push_back(row(L"+0x21\tBody\t" + num(s.body) + L" " + body_name(s.body),
                          kText, ship_node(NodeKind::kBody, s.body)));
    v.lines.push_back(row(L"+0x23\tAuthor\t" + widen(s.author)));
    v.lines.push_back(row(L"+0x38\tSound file\t" + widen(s.sound_file)));
    v.lines.push_back(row(L""));
    if (const BodyRecord* b = g.ships->body(s)) {
        body_lines(v, *b, s.body);
        body_picture(v, *b);
    } else {
        v.text.push_back(L"The body index is out of range.");
    }
}

void view_bodies(View& v) {
    v.title = L"MASTER.BOD: ship bodies";
    need_ships();
    v.kind = ViewKind::kBodies;
    const auto& recs = g.ships->bodies.records;
    v.subtitle = num(static_cast<long long>(recs.size())) + L" records of 44 bytes, " +
                 num(static_cast<long long>(g.ships->bodies.tail.size())) +
                 L" bytes after them; double-click a row to open it";
    v.columns = {{L"#", 0}, {L"Name", 5}, {L"Bitmap", 21}, {L"Strength", 37}, {L"Flags", 47},
                 {L"Ships", 72}};
    for (std::size_t i = 0; i < recs.size(); ++i) {
        const BodyRecord& b = recs[i];
        long long users = 0;
        for (const ShipRecord& s : g.ships->ships.records) users += s.body == i;
        v.lines.push_back(row(num(static_cast<long long>(i)) + L"\t" + widen(b.name) + L"\t" +
                                  (b.bitmap_id == kBodyBitmapFromFile ? widen(b.bitmap_file) + L".DIB"
                                                                      : L"#4000 " + num(b.bitmap_id)) +
                                  L"\t" + num(b.strength_base) + L"\t" + body_flags(b.flags) + L"\t" +
                                  num(users),
                              b.name.empty() ? kDim : kText, ship_node(NodeKind::kBody, static_cast<int>(i))));
    }
}

void view_body(View& v, int index) {
    need_ships();
    const BodyRecord& b = g.ships->bodies.records.at(static_cast<std::size_t>(index));
    v.kind = ViewKind::kBody;
    v.title = L"MASTER.BOD" + std::wstring(kSep) + num(index) + L" " + quoted(widen(b.name));
    v.subtitle = L"Every field of the body record (src/formats/ships.h)";
    v.columns = {{L"Offset", 0}, {L"Field", 7}, {L"Value", 21}};
    body_lines(v, b, static_cast<std::size_t>(index));
    v.lines.push_back(row(L""));
    v.lines.push_back(row(L"\tShips with this body (double-click to open)", kAccent));
    for (std::size_t i = 0; i < g.ships->ships.records.size(); ++i)
        if (g.ships->ships.records[i].body == index)
            v.lines.push_back(row(L"\t" + num(static_cast<long long>(i)) + L"\t" +
                                      widen(g.ships->ships.records[i].name),
                                  kText, ship_node(NodeKind::kShip, static_cast<int>(i))));
    body_picture(v, b);
}

}  // namespace
