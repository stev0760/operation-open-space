#pragma once
// The Win16 look of a WS_BORDER edit control in a dialog built from a Win16 template.
//
// A version-4 Win32 dialog gives a WS_BORDER edit a 2-pixel client edge (WS_EX_CLIENTEDGE); the
// Win16 dialog manager leaves WS_BORDER, and the edit draws a 1-pixel border inside its window
// and places its text inside that border (start-0001: Restore's 330, New Game's PILOT, #799's
// 801, Order's 701; m2-accept-0001 D1) [ORACLE start-0001] [ORACLE m2-accept-0001].

#ifdef _WIN32
#include <windows.h>

namespace is::gfx {

// While it lives, the dialog the thread creates next gets its WS_BORDER edits and list boxes as
// the Win16 dialog manager makes them: a thread CBT hook takes WS_EX_CLIENTEDGE off each Edit or
// ListBox child of that dialog, and off the CREATESTRUCT, at the control's WM_NCCREATE, and
// keeps WS_BORDER, so the control has a 1-pixel border and its text inside it, as in the oracle
// (the list boxes of the Ambulance Options and Sell dialogs: core-0003; M4 s4 fixer B). Taking
// the client edge off after creation leaves the text 1 px up and left (m2-accept-0001 D1),
// because the edit decides at WM_NCCREATE whether it draws a border [DESIGN]. Construct it just
// before DialogBoxIndirectParam or CreateDialogIndirectParam. Nests: one hook per thread, owned
// by the outermost scope; an inner scope watches its own dialog until it ends.
class Win16EditScope {
public:
    Win16EditScope();
    ~Win16EditScope();
    Win16EditScope(const Win16EditScope&) = delete;
    Win16EditScope& operator=(const Win16EditScope&) = delete;

private:
    HHOOK hook_ = nullptr;
    HWND saved_dialog_ = nullptr;
    bool saved_active_ = false;
};

}  // namespace is::gfx
#endif
