// Win16 resource decoders (STRING, DIALOG, MENU, ACCELERATOR) and their conversion to the
// Win32 in-memory templates that DialogBoxIndirectParamW, LoadMenuIndirectW and
// CreateAcceleratorTableW take. Win16 layouts: docs/re/resources.md#win16-layouts.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "formats/ne.h"

namespace is::fmt {

// ---- strings: block (id >> 4) + 1, 16 length-prefixed slots

std::optional<std::string> load_string(const NeModule& m, std::uint16_t id);
std::map<std::uint16_t, std::string> all_strings(const NeModule& m);

// ---- dialogs

struct NameOrOrd {
    bool is_ord = false;
    std::uint16_t ord = 0;
    std::string name;  // cp1252
    bool empty() const { return !is_ord && name.empty(); }
};

struct DialogItem16 {
    std::int16_t x = 0, y = 0, cx = 0, cy = 0;
    std::uint16_t id = 0;
    std::uint32_t style = 0;
    std::uint8_t class_atom = 0;  // 0x80-0x85 for BUTTON EDIT STATIC LISTBOX SCROLLBAR COMBOBOX
    std::string class_name;       // when class_atom is 0
    NameOrOrd text;
    std::vector<std::uint8_t> extra;
};

struct Dialog16 {
    std::uint32_t style = 0;
    std::int16_t x = 0, y = 0, cx = 0, cy = 0;
    NameOrOrd menu;
    std::string class_name;
    std::string caption;
    std::uint16_t point_size = 0;  // with DS_SETFONT
    std::string face;
    std::vector<DialogItem16> items;
};

constexpr std::uint32_t kDsSetFont = 0x40;

Dialog16 parse_dialog16(const std::uint8_t* p, std::size_t n);
// A DWORD-aligned Win32 DLGTEMPLATE (not DLGTEMPLATEEX) with the same styles, geometry
// and texts; `or_style` is ORed into the dialog style (a viewer may add DS_NOFAILCREATE).
std::vector<std::uint8_t> dialog_to_win32(const Dialog16& d, std::uint32_t or_style = 0);

// ---- menus

struct MenuItem16 {
    std::uint16_t flags = 0;  // MF_* (MF_POPUP 0x10, MF_END 0x80)
    std::uint16_t id = 0;     // absent for popups
    std::string text;
    std::vector<MenuItem16> children;
};
constexpr std::uint16_t kMfPopup = 0x10, kMfEnd = 0x80;

std::vector<MenuItem16> parse_menu16(const std::uint8_t* p, std::size_t n);
std::vector<std::uint8_t> menu_to_win32(const std::vector<MenuItem16>& items);

// ---- accelerators: Win16 5-byte entries; flag 0x80 marks the last

struct Accel16 {
    std::uint8_t flags = 0;  // FVIRTKEY 1, FNOINVERT 2, FSHIFT 4, FCONTROL 8, FALT 0x10
    std::uint16_t key = 0;
    std::uint16_t cmd = 0;
};
std::vector<Accel16> parse_accel16(const std::uint8_t* p, std::size_t n);

}  // namespace is::fmt
