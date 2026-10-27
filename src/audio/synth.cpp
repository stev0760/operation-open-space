// The Synth interface's out-of-line destructor (ADR 0011 decision 3).
#include "audio/synth.h"

namespace is::audio::music {
Synth::~Synth() = default;
}  // namespace is::audio::music
