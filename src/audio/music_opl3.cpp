// The opl3 synth factory (ADR 0011 decision 3). [OPEN] The CMake option IS_OPL3 is OFF and no
// OPL3 synth is built in (the reason is in audio/music_opl3.h), so the factory returns nullptr as
// audio/synth.h allows for a synth that is not built in. [DESIGN]
#include "audio/music_opl3.h"

#include "audio/synth.h"

namespace is::audio::music {

std::unique_ptr<Synth> make_opl3() {
    static_assert(!kOpl3BuiltIn, "an OPL3 synth needs its implementation here");
    return nullptr;
}

}  // namespace is::audio::music
