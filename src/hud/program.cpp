#include "hud/program.h"

namespace is::hud {

namespace {
Links g_links;
Hud* g_hud = nullptr;
std::function<void(int)> g_scroll_pos;
std::function<void()> g_close;
}  // namespace

Links& links() { return g_links; }
Hud* hud() { return g_hud; }

void bind_program(Hud* hud, std::function<void(int)> scroll_pos, std::function<void()> close) {
    g_hud = hud;
    g_scroll_pos = std::move(scroll_pos);
    g_close = std::move(close);
}

void wave_start() {
    if (g_hud) g_hud->info().set_mode(kModeTarget);  // seg9:0x60bc: DGROUP:0xa76 = 2
}

void vscroll(int code, int pos) {
    if (g_links.vscroll) g_links.vscroll(code, pos);
}

void set_scroll_pos(int pos) {
    if (g_scroll_pos) g_scroll_pos(pos);
}

void close() {
    if (g_close) g_close();
}

}  // namespace is::hud
