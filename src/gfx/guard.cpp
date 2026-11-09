#include "gfx/guard.h"

#include <utility>

namespace is::gfx {

namespace {
GuardTodo& guard_fn() {
    static GuardTodo f;
    return f;
}
}  // namespace

void set_guard_todo(GuardTodo f) { guard_fn() = std::move(f); }

void guard_todo(bool& once, const char* what, const char* address) {
    if (once) return;
    once = true;
    if (guard_fn()) guard_fn()(what, address);
}

}  // namespace is::gfx
