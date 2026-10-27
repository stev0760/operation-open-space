// The opl3 music synth of ADR 0011 decision 3 (an OPL3 emulator with a Windows-era Sound Blaster
// FM bank behind the port's MIDI player). make_opl3() is declared in audio/synth.h.
//
// [OPEN] Not built in: libADLMIDI, the candidate the ADR names, is GPL-3.0-or-later in its core
// (adlmidi.cpp, adlmidi_midiplay.cpp, adlmidi_opl3.cpp, adlmidi_private.cpp, adlmidi_load.cpp and
// include/adlmidi.h, at tag v1.6.2.1 and every earlier tag checked), so it cannot be vendored
// under the port's third-party rule (MIT, BSD, zlib, public domain or LGPL only), and its bank
// named after the Windows 3.x AdLib/Sound Blaster driver ("WOPL (MS-AdLib, Windows 3.x)") is a
// grey-zone bank without a licence. The CMake option IS_OPL3 stays OFF until another route is
// decided; make_opl3() returns nullptr and the session keeps the mci or soundfont backend.
#pragma once

namespace is::audio::music {

// False: make_opl3() returns nullptr in this build (IS_OPL3 is OFF).
constexpr bool kOpl3BuiltIn = false;

}  // namespace is::audio::music
