// The opl3 synth factory (ADR 0011 decision 3, src/audio/music_opl3.*). While the CMake option
// IS_OPL3 is OFF the factory reports "not built in" (nullptr), which the backend selection reads
// as "opl3 unavailable"; the synth cases (open without a file, the bank, a note renders, panic
// silences, 22050 and 11025 Hz) join this file with the implementation.
#include "doctest.h"

#include "audio/music_opl3.h"
#include "audio/synth.h"

using namespace is::audio::music;

TEST_CASE("opl3: make_opl3 is nullptr exactly when the synth is not built in") {
    auto synth = make_opl3();
    CHECK((synth != nullptr) == kOpl3BuiltIn);
    if (!synth) {
        MESSAGE("opl3 synth not built in (IS_OPL3 OFF): synth cases skipped");
        return;
    }
}
