// The ADR 0016 guard log of the ISPACE.DLL ordinals in src/gfx: a read past a table, a buffer or
// a record logs once per site through the program's todo facility (world/world.h guard_todo
// for the world). Unbound, nothing is logged [DESIGN ADR 0016 decisions 2 and 7]. Portable.
#pragma once

#include <functional>
#include <string>

namespace is::gfx {

using GuardTodo = std::function<void(const std::string& what, const char* address)>;
void set_guard_todo(GuardTodo f);
// Logs (what, address) the first time `once` is false, then sets it.
void guard_todo(bool& once, const char* what, const char* address);

}  // namespace is::gfx
