// The shared lightning bolt (bolt.h): the parts that are not templates.
#include "shell/bolt.h"

namespace is::shell {

// [BYTES seg8:0x1290]
int bolt_newton(long g, long q) {
    if (g < 3) g = 3;
    return static_cast<int>((q / g + g) >> 1);
}

// [BYTES seg12:0x7256]
bool bolt_line_visible(int ax, int ay, int bx, int by, int w, int play_h) {
    if (ay < 0 && by < 0) return false;
    if (ay > play_h && by > play_h) return false;
    if (ax < 0 && bx < 0) return false;
    if (ax > w && bx > w) return false;
    return true;
}

}  // namespace is::shell
