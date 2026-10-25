#include "shell/rng.h"

namespace is::shell {

Rng& game_rng() {
    static Rng r;
    return r;
}

}  // namespace is::shell
