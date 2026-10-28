// The demo pilot seg6:0x1e68 (see pilot.h).
#include "nav/pilot.h"

namespace is::nav {

void Pilot::step(bool arg, std::uint32_t now, std::uint32_t tick, PilotTarget& t) {
    if (arg) next_ = now + 1200u;  // GetCurrentTime() + 0x4b0, add/adc [BYTES seg6:0x1e7b]
    done_ = false;                  // [BYTES seg6:0x1e8d]
    if (!(next_ < tick)) return;    // unsigned 32-bit compare [BYTES seg6:0x1e9b]
    next_ = tick + 600u;            // [BYTES seg6:0x1ea2]
    if (t.enter() == 0) {           // [BYTES seg6:0x1eb0]
        const int sel = t.selection();
        if (sel < t.count()) {      // signed [BYTES seg6:0x1ecb]
            if (t.sub_digit() == 1) t.expand();  // [BYTES seg6:0x1ed8 seg6:0x1ede]
            t.select(sel + 1);      // [BYTES seg6:0x1f04]
        } else {
            done_ = true;
        }
    }
    if (t.count() - 1 <= t.selection()) done_ = true;  // [BYTES seg6:0x1f0a seg6:0x1f14]
}

namespace {
Pilot g_pilot;
PilotTarget* g_target = nullptr;
}  // namespace

Pilot& pilot() { return g_pilot; }
void set_pilot_target(PilotTarget* t) { g_target = t; }

bool pilot_open(std::uint32_t now, std::uint32_t tick) {
    if (!g_target) return false;
    g_pilot.step(true, now, tick, *g_target);
    return true;
}

bool pilot_frame(std::uint32_t tick) {
    if (!g_target) return false;
    g_pilot.step(false, 0, tick, *g_target);
    return true;
}

std::string demo_save_path(const std::string& local_path, const std::string& games) {
    // lstrcpy(LocalPath), lstrcat(Games), lstrcat("demogame.sav") [BYTES seg6:0x92b8]; Games
    // ends in '\' (seg6:0x6d8) [BYTES seg6:0x9a0].
    std::string p = local_path;
    if (!p.empty() && p.back() != '\\') p += '\\';  // ADR 0004 gives LocalPath without it [DESIGN]
    std::string g = games;
    if (!g.empty() && g.back() != '\\') g += '\\';
    return p + g + kDemoSave;
}

bool deletes_demo_save(const DemoEnd& e) {
    if (e.timer == 0 || e.timer > e.tick) return false;  // [BYTES seg7:0x610 seg7:0x622]
    if (e.player) return false;  // [BYTES seg7:0x62b]
    return e.record_complete && e.pilot_done;  // [BYTES seg7:0x63e seg7:0x646 seg7:0x64d]
}

}  // namespace is::nav
