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

// seg9:0x60bc's HUD part: seg3:0x45a(2) picks the first icon target, then DGROUP:0xa76 = 2
// (shell.md "Game-start dialogs", world.md "Retarget"; M3 s3 fixer) [BYTES seg9:0x64ac
// seg9:0x64ae seg9:0x64c2] [ORACLE wave-0001: BRAVO.EXE "Icon 1" at wave start]. seg8:0x4d94
// before it is not ported [OPEN seg8:0x4d94].
void wave_start() {
    if (!g_hud) return;
    g_hud->info().select_target(2);
    g_hud->info().set_mode(kModeTarget);
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
