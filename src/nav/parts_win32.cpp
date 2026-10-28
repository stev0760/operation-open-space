// The race and duel boxes, Enter and the demo pilot plugged into the IS_NAV window (enter.h
// install_parts, nav.h Parts). Win32 only.
#ifdef _WIN32
#include "nav/boxes.h"
#include "nav/enter.h"
#include "nav/nav.h"
#include "nav/pilot.h"

#include <exception>
#include <string>

#include "settings/settings.h"

namespace is::nav {

namespace {

EnterHost g_eh;
EnterState g_state;
int g_ext = 0;  // DGROUP:0x94b8

void log(const std::string& s) {
    if (g_eh.log) g_eh.log(s);
}

std::string describe(const Handover& h) {
    return "\"" + h.directory + "\" 0x92e8=" + std::to_string(h.demons_gate) +
           " 0x94e=" + std::to_string(h.race) + " 0x950=" + std::to_string(h.duel) +
           " 0x94a8=" + std::to_string(h.difficulty) + " 0x94aa=" + std::to_string(h.opponent) +
           " 0x9ea0=" + std::to_string(h.laps);
}

// The rest of seg6:0x301a after the window's DGROUP:0xd20 and DGROUP:0x90f4 tests.
int enter_part() {
    HWND nav = window();
    Walker* w = walker();
    if (!nav || !w) return 0;
    EnterInputs in;
    in.armed = armed();
    in.state = 0;  // tested by nav::enter()
    in.game = game();
    in.command_line = g_eh.command_line && g_eh.command_line() != 0;
    in.demo = g_eh.demo && g_eh.demo() != 0;
    in.demo_type = g_eh.demo_type ? g_eh.demo_type() : kDemoTypeDefault;
    in.boxes = read_boxes(nav);
    // DGROUP:0x91f8 and 0x92e4 decide the EnterAction in nav::accept().
    std::string& dir = directory();
    const EnterResult r = check(g_state, dir, in, *w, strings(), records(), g_eh.ini,
                                g_eh.rand ? g_eh.rand : std::function<int()>([] { return 0; }));
    if (!r.ini_writes.empty()) {
        std::string keys;
        for (const auto& kv : r.ini_writes) keys += " " + kv.first + "=" + std::to_string(kv.second);
        log("navigation Enter: INI" + keys);
        if (g_eh.ini_changed) {
            try {
                g_eh.ini_changed();
            } catch (const std::exception& x) {
                log(std::string("navigation Enter: INI: ") + x.what());
            }
        }
    }
    if (!r.accepted) {
        log("navigation Enter refused: " + dir);
        return 0;
    }
    if (!record_room()) {  // accept() would refuse: no hand-over (M3 s1 fixer) [OPEN]
        log("navigation Enter refused: no room for a directory record of " + dir + " [OPEN]");
        return 0;
    }
    log("navigation Enter: hand-over " + describe(r.handover));
    if (g_eh.handover) g_eh.handover(r.handover);
    return accept(r.handover.directory);  // seg6:0x3686
}

// The window as the demo pilot sees it.
class WindowTarget final : public PilotTarget {
public:
    int enter() override { return nav::enter(); }
    int count() override { return row_count(); }
    int selection() override { return selected(); }
    int sub_digit() override {
        Tree* t = tree();
        return t ? t->sub_digit(selected()) : 0;
    }
    void expand() override { expand_or_collapse(); }  // called only for sub digit 1
    void select(int row) override {
        if (window()) select_row(row);
    }
};
WindowTarget g_target;

}  // namespace

EnterState& enter_state() { return g_state; }

void install_parts(const EnterHost& host) {
    g_eh = host;
    Parts p;
    p.create_boxes = [](HWND nav, int cw, int ch, HFONT font) {
        HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(nav, GWLP_HINSTANCE));
        g_ext = create_boxes(nav, inst, font, g_eh.lang, g_eh.ini);
        place_boxes(nav, cw, ch, g_ext);
    };
    p.layout_boxes = [](HWND nav, int cw, int ch) { place_boxes(nav, cw, ch, g_ext); };
    p.show_box = [](HWND nav, int kind) {
        show_boxes(nav, kind == 1 ? BoxKind::kRace : kind == 2 ? BoxKind::kDuel : BoxKind::kNone);
    };
    p.enter = [] { return enter_part(); };
    p.demo_pilot = [](int arg) {
        const std::uint32_t tick = g_eh.tick ? g_eh.tick() : 0;
        const std::uint32_t now = g_eh.now ? g_eh.now() : tick;
        if (arg) pilot_open(now, tick);
        else pilot_frame(tick);
    };
    p.initial = [](const InitialSelection& s) {
        // seg6:0x37ca clears DGROUP:0x94e and 0x950; DemoType 521 or 522 sets one [BYTES
        // seg6:0x37ca seg6:0x3d00 seg6:0x3d56].
        g_state.handover.race = s.race;
        g_state.handover.duel = s.duel;
    };
    set_parts(p);
    set_pilot_target(&g_target);
}

}  // namespace is::nav
#endif
