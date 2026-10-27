// The opl3 synth factory and synth (src/audio/music_opl3.*, ADR 0011 decision 3). With the
// CMake option IS_OPL3 off the factory reports "not built in" (nullptr), which the backend
// selection reads as "opl3 unavailable", and the synth cases skip.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <vector>

#include "audio/music_opl3.h"
#include "audio/synth.h"
#include "doctest.h"

using namespace is::audio::music;

namespace {

std::vector<std::int16_t> render(Synth& s, std::size_t frames) {
    std::vector<std::int16_t> out(frames * 2, 0x5a5a);
    s.render(out.data(), frames);
    return out;
}

int peak(const std::vector<std::int16_t>& v) {
    int p = 0;
    for (auto x : v) p = std::max(p, std::abs(static_cast<int>(x)));
    return p;
}

void send(Synth& s, std::initializer_list<std::uint8_t> msg) {
    const std::vector<std::uint8_t> b(msg);
    s.midi(b.data(), b.size());
}

}  // namespace

TEST_CASE("opl3: make_opl3 is nullptr exactly when the synth is not built in") {
    auto synth = make_opl3();
    CHECK((synth != nullptr) == kOpl3BuiltIn);
    if (!synth) MESSAGE("opl3 synth not built in (IS_OPL3 OFF): synth cases skipped");
}

TEST_CASE("opl3 synth: open without a file, a note sounds, reset silences, both rates") {
    auto s = make_opl3();
    if (!s) return;
    CHECK(s->name() == "opl3");
    CHECK(peak(render(*s, 100)) == 0);  // not open: silence, and render replaces
    SynthConfig bad;
    bad.rate = 0;
    CHECK_FALSE(s->open(bad));
    for (std::uint32_t rate : {22050u, 11025u}) {
        CAPTURE(rate);
        SynthConfig cfg;  // no SoundFont: the opl3 synth ignores it
        cfg.rate = rate;
        cfg.soundfont = "no-such-file.sf2";
        REQUIRE(s->open(cfg));
        CHECK(peak(render(*s, rate / 10)) == 0);
        send(*s, {0x90, 60, 110});
        send(*s, {0x99, 42, 110});  // closed hi-hat
        const auto on = render(*s, rate / 2);
        CHECK(peak(on) > 500);
        s->reset();
        render(*s, rate / 50);
        CHECK(peak(render(*s, rate / 10)) == 0);
        // A held note ends at a GM System On as well.
        send(*s, {0xc0, 48});
        send(*s, {0x90, 64, 120});
        CHECK(peak(render(*s, rate / 4)) > 500);
        send(*s, {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7});
        render(*s, rate / 50);
        CHECK(peak(render(*s, rate / 10)) == 0);
    }
}
