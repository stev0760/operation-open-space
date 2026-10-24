// isview part 2 of 6: the game directory's NE modules, the identification, the ships and the
// tree that lists them. Included once, by main.cpp.

namespace {

// ---- modules (ModuleInfo, scan_modules and the rest are in dump_json.h)

const ModuleInfo* find_module(const std::string& name) { return isv::find_module(g.modules, name); }

bool is_language_module(const ModuleInfo& m) {
    return ascii_ieq(m.name, g.store->language_module());
}

// ---- start-up

void note(const std::wstring& what, const std::exception& x) {
    g.load_notes.push_back(what + L": " + message(x.what()));
}

void load_game() {
    g.modules = isv::scan_modules(*g.store);
    try {
        g.ident = isa::identify(g.store->files());
    } catch (const std::exception& x) {
        g.ident_error = x.what();
    }
    try {
        const VersionInfo v = g.store->version(isa::kMainProgram);
        if (const std::string* s = v.find_string("FileVersion")) g.version = *s;
    } catch (const std::exception& x) {
        note(L"version", x);
    }
    try {
        g.ships = g.store->ships();
    } catch (const std::exception& x) {
        g.ships_error = x.what();
    }

    // v0: the language module's title, menu bar and accelerators
    try {
        if (auto t = g.store->find_string(kTitleString)) g.title = widen(*t);
    } catch (const std::exception& x) {
        note(L"title", x);
    }
    try {
        const auto t = g.store->menu_template(kMainMenu);
        g.menu = LoadMenuIndirectW(reinterpret_cast<const MENUTEMPLATEW*>(t.data()));
    } catch (const std::exception& x) {
        note(L"menu bar", x);
    }
    try {
        std::vector<ACCEL> a;
        for (const Accel16& e : g.store->accelerators(kMainAccel))
            a.push_back(ACCEL{static_cast<BYTE>(e.flags), e.key, e.cmd});
        if (!a.empty()) g.accel = CreateAcceleratorTableW(a.data(), static_cast<int>(a.size()));
    } catch (const std::exception& x) {
        note(L"accelerators", x);
    }

    // v0: IGRAPHIC.DLL's sprites for the overview
    try {
        const NeModule& gfx = g.store->module(isa::kGraphicsDll);
        for (const NeResource* r : gfx.all(isa::kSpriteType)) {
            if (named_id(*r)) continue;
            const Dib d = g.store->sprite(r->id);
            g.sprites.push_back({r->id, dib_image(d, sprite_transparent(d), Backdrop::kBackground)});
        }
        std::sort(g.sprites.begin(), g.sprites.end(),
                  [](const Sprite& a, const Sprite& b) { return a.id < b.id; });
        for (const Sprite& s : g.sprites)
            if (s.id == kXformSprite) g.xform = &s;
    } catch (const std::exception& x) {
        note(L"sprites", x);
    }
}

// ---- ships

const wchar_t* to_friends_label(std::uint8_t v) {
    static const wchar_t* const k[] = {L"LOYAL", L"HELPFUL", L"FRIENDLY", L"DISINTERESTED"};
    return v < 4 ? k[v] : L"?";
}

const wchar_t* to_enemies_label(std::uint8_t v) {
    static const wchar_t* const k[] = {L"PSYCHOPATHIC", L"HOSTILE", L"UNFRIENDLY", L"DISINTERESTED"};
    return v < 4 ? k[v] : L"?";
}

std::wstring flag_names(std::uint8_t f, std::initializer_list<std::pair<std::uint8_t, const wchar_t*>> names) {
    std::wstring s = hex(f, 2);
    for (const auto& n : names)
        if (f & n.first) s += L" " + std::wstring(n.second);
    return s;
}

std::wstring ship_flags(std::uint8_t f) {
    return flag_names(f, {{kShipCombo180, L"combo180"}, {kShipRepair, L"Repair"},
                          {kShipCloak, L"Cloak"}, {kShipCapturesIcons, L"CapturesIcons"},
                          {kShipGenderMale, L"Male"}});
}

std::wstring body_flags(std::uint8_t f) {
    return flag_names(f, {{kBodyThrust, L"Thrust"}, {kBodyShield, L"Shield"},
                          {kBodyPullOffWings, L"PullOffWings"}, {kBodyThrustBehind, L"ThrustBehind"},
                          {kBodyFlag40, L"flag40"}});
}

std::wstring weapon_label(std::uint8_t w) {
    std::wstring s = num(w);
    try {
        if (auto t = g.store->find_string(static_cast<std::uint16_t>(kWeaponString + w)))
            s += L" \u201C" + widen(*t) + L"\u201D";
    } catch (const std::exception&) {
    }
    return s;
}

std::wstring body_name(std::uint16_t index) {
    if (!g.ships || index >= g.ships->bodies.records.size()) return L"(out of range)";
    return widen(g.ships->bodies.records[index].name);
}

// ---- the tree

int add_node(Node n) {
    g.nodes.push_back(std::move(n));
    return static_cast<int>(g.nodes.size()) - 1;
}

// The length before the trailing NULs when every byte before them is printable or a line
// break or tab, else 0: such a resource is shown as text.
std::size_t text_length(const std::uint8_t* p, std::size_t n) {
    std::size_t end = n;
    while (end > 0 && p[end - 1] == 0) --end;
    for (std::size_t i = 0; i < end; ++i)
        if (p[i] < 0x20 && p[i] != '\r' && p[i] != '\n' && p[i] != '\t') return 0;
    return end;
}

std::wstring describe(const ModuleInfo& mi, const NeResource& r) {
    const NeModule& m = *mi.ne;
    try {
        switch (int_type(r)) {
        case RT16_STRING: {
            if (r.id == 0) return {};
            const unsigned first = (r.id - 1u) * 16u;
            return L"ids " + num(first) + L"\u2013" + num(first + 15);
        }
        case RT16_DIALOG:
            return L"\u201C" + widen(parse_dialog16(m.data(r), m.size(r)).caption) + L"\u201D";
        case RT16_MENU:
            return num(static_cast<long long>(parse_menu16(m.data(r), m.size(r)).size())) +
                   L" top-level items";
        case RT16_ACCELERATOR:
            return num(static_cast<long long>(parse_accel16(m.data(r), m.size(r)).size())) +
                   L" entries";
        case RT16_BITMAP:
            return dims(parse_dib(m.data(r), m.size(r)));
        case RT16_ICON:
            return dims(parse_icon(m.data(r), m.size(r)).image);
        case RT16_GROUP_ICON:
            return num(static_cast<long long>(parse_group_icon(m.data(r), m.size(r)).size())) +
                   L" images";
        case RT16_VERSION: {
            const VersionInfo v = parse_version16(m.data(r), m.size(r));
            if (const std::string* s = v.find_string("FileVersion")) return widen(*s);
            return v.fixed ? widen(v.fixed->file_version()) : std::wstring();
        }
        default:
            break;
        }
    } catch (const std::exception&) {
        return L"(cannot decode)";
    }
    // custom types: described only when the bytes are what the type usually holds
    try {
        if (int_type(r) == isa::kSpriteType) return dims(parse_dib(m.data(r), m.size(r)));
        if (int_type(r) == kWavResourceType && m.size(r) >= 4 &&
            std::memcmp(m.data(r), "RIFF", 4) == 0) {
            const auto f = wav_file(m, r);
            const Wave w = parse_wav(f.data(), f.size());
            return w.rate ? num(static_cast<long long>((w.frames() * 1000 + w.rate / 2) / w.rate)) + L" ms" : L"";
        }
    } catch (const std::exception&) {
    }
    if (const std::size_t t = text_length(m.data(r), m.size(r))) {
        constexpr std::size_t kSnippet = 40;
        const std::string s(reinterpret_cast<const char*>(m.data(r)), std::min(t, kSnippet));
        return L"\u201C" + escaped(widen(s)) + (t > kSnippet ? L"\u2026\u201D" : L"\u201D");
    }
    return {};
}

void build_tree() {
    Node n;
    n.kind = NodeKind::kOverview;
    n.label = L"Overview";
    n.note = L"title, Transformer, sprites";
    n.key = L"overview";
    add_node(n);

    n = Node{};
    n.kind = NodeKind::kIdentify;
    n.label = L"Identification";
    n.key = L"identify";
    if (g.ident) {
        const std::size_t unknown = g.ident->unknown_count();
        n.note = num(static_cast<long long>(g.ident->files.size())) + L" files, " +
                 (unknown ? num(static_cast<long long>(unknown)) + L" unknown" : L"all known");
    } else {
        n.note = L"failed";
    }
    add_node(n);

    n = Node{};
    n.kind = NodeKind::kShips;
    n.label = L"MASTER.SHP";
    n.key = L"ships";
    const int ships = add_node(n);
    n = Node{};
    n.kind = NodeKind::kBodies;
    n.label = L"MASTER.BOD";
    n.key = L"bodies";
    if (g.ships) {
        g.nodes[ships].note = num(static_cast<long long>(g.ships->ships.records.size())) + L" ships";
        g.nodes[ships].has_children = !g.ships->ships.records.empty();
        for (std::size_t i = 0; i < g.ships->ships.records.size(); ++i) {
            Node c;
            c.kind = NodeKind::kShip;
            c.level = 1;
            c.parent = ships;
            c.index = static_cast<int>(i);
            c.label = num(static_cast<long long>(i));
            c.key = c.label;
            c.note = widen(g.ships->ships.records[i].name);
            add_node(c);
        }
        n.note = num(static_cast<long long>(g.ships->bodies.records.size())) + L" bodies";
        n.has_children = !g.ships->bodies.records.empty();
    } else {
        g.nodes[ships].note = L"not read";
        n.note = L"not read";
    }
    const int bodies = add_node(n);
    if (g.ships) {
        for (std::size_t i = 0; i < g.ships->bodies.records.size(); ++i) {
            Node c;
            c.kind = NodeKind::kBody;
            c.level = 1;
            c.parent = bodies;
            c.index = static_cast<int>(i);
            c.label = num(static_cast<long long>(i));
            c.key = c.label;
            c.note = widen(g.ships->bodies.records[i].name);
            add_node(c);
        }
    }

    for (std::size_t mi = 0; mi < g.modules.size(); ++mi) {
        const ModuleInfo& m = g.modules[mi];
        Node mn;
        mn.kind = NodeKind::kModule;
        mn.module = static_cast<int>(mi);
        mn.label = widen(m.name);
        mn.key = mn.label;
        mn.note = m.ne ? widen(m.ne->module_name()) : L"cannot load";
        mn.has_children = m.ne && !m.ne->resources().empty();
        const int mnode = add_node(mn);
        if (!m.ne) continue;

        // types in resource-table order, ids sorted: integers first, then names
        std::vector<const NeResource*> types;
        for (const NeResource& r : m.ne->resources())
            if (std::none_of(types.begin(), types.end(),
                             [&](const NeResource* t) { return same_type(*t, r); }))
                types.push_back(&r);
        for (const NeResource* t : types) {
            std::vector<const NeResource*> rs;
            for (const NeResource& r : m.ne->resources())
                if (same_type(r, *t)) rs.push_back(&r);
            std::stable_sort(rs.begin(), rs.end(), [](const NeResource* a, const NeResource* b) {
                if (named_id(*a) != named_id(*b)) return !named_id(*a);
                return named_id(*a) ? a->name < b->name : a->id < b->id;
            });
            Node tn;
            tn.kind = NodeKind::kType;
            tn.level = 1;
            tn.parent = mnode;
            tn.module = static_cast<int>(mi);
            tn.res = t;
            tn.label = widen(type_label(*t));
            tn.key = tn.label;
            tn.note = num(static_cast<long long>(rs.size()));
            tn.has_children = true;
            const int tnode = add_node(tn);
            for (const NeResource* r : rs) {
                Node rn;
                rn.kind = NodeKind::kResource;
                rn.level = 2;
                rn.parent = tnode;
                rn.module = static_cast<int>(mi);
                rn.res = r;
                rn.label = widen(id_label(*r));
                rn.key = rn.label;
                rn.note = describe(m, *r);
                add_node(rn);
            }
        }
    }
}

// The node at PATH: overview, identify, ships[/N], bodies[/N] or MODULE[/TYPE[/ID]], each part
// compared without case; -1 when there is none.
int find_node(const std::wstring& path) {
    std::vector<std::wstring> parts;
    std::size_t start = 0;
    for (;;) {
        const std::size_t slash = path.find(L'/', start);
        parts.push_back(lower(path.substr(start, slash - start)));
        if (slash == std::wstring::npos) break;
        start = slash + 1;
    }
    int parent = -1, found = -1;
    for (const std::wstring& part : parts) {
        found = -1;
        for (int i = 0; i < static_cast<int>(g.nodes.size()); ++i)
            if (g.nodes[i].parent == parent && lower(g.nodes[i].key) == part) {
                found = i;
                break;
            }
        if (found < 0) return -1;
        parent = found;
    }
    return found;
}

// The tree node of this resource, or -1 (also for nullptr).
int resource_node(const NeResource* r) {
    if (!r) return -1;
    for (int i = 0; i < static_cast<int>(g.nodes.size()); ++i)
        if (g.nodes[i].kind == NodeKind::kResource && g.nodes[i].res == r) return i;
    return -1;
}

int ship_node(NodeKind kind, int index) {
    for (int i = 0; i < static_cast<int>(g.nodes.size()); ++i)
        if (g.nodes[i].kind == kind && g.nodes[i].index == index) return i;
    return -1;
}

}  // namespace
