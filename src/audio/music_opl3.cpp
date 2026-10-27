// The opl3 synth (ADR 0011 decision 3): Nuked OPL3 under the port's General MIDI voice driver
// with the Fat Man bank. It stands in for no routine of the original: the original hands its
// music to the MCI sequencer [BYTES ISPACE.DLL seg9:0x3906], and on a 1994 Sound Blaster the
// MIDI mapper sent it to the card's OPL3; this synth replaces that card and its Windows FM
// driver. [DESIGN] Portable: no Win32 calls.
#include "audio/music_opl3.h"

#include "audio/synth.h"

#if defined(IS_OPL3) && IS_OPL3

#include <cstring>
#include <limits>
#include <mutex>

#include "audio/opl3_bank.h"
#include "audio/opl3_driver.h"
#include "opl3.h"

namespace is::audio::music {

namespace {

class Opl3Synth final : public Synth {
public:
    Opl3Synth()
        : driver_(fatman_bank(), [this](std::uint16_t reg, std::uint8_t v) {
              // The reset's writes land at once on the freshly reset chip; the notes' writes go
              // through Nuked's write buffer, which spaces them two chip samples apart as the
              // real chip's write timing does, so a stolen voice's key-off reaches the envelope
              // before its new key-on and the note retriggers. [DESIGN]
              if (direct_)
                  OPL3_WriteReg(&chip_, reg, v);
              else
                  OPL3_WriteRegBuffered(&chip_, reg, v);
          }) {
        std::memset(&chip_, 0, sizeof chip_);
        driver_.set_gain_steps(kOpl3GainSteps);
    }

    bool open(const SynthConfig& cfg) override {  // the SoundFont path is ignored
        std::lock_guard<std::mutex> lock(mu_);
        if (cfg.rate == 0 || cfg.rate > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
            return false;
        rate_ = cfg.rate;
        reset_locked();
        return true;
    }

    void reset() override {
        std::lock_guard<std::mutex> lock(mu_);
        if (rate_) reset_locked();
    }

    void midi(const std::uint8_t* m, std::size_t n) override {
        if (!m || n == 0) return;
        std::lock_guard<std::mutex> lock(mu_);
        if (!rate_) return;
        if (Opl3Driver::is_reset_sysex(m, n)) {
            reset_locked();
            return;
        }
        driver_.midi(m, n);
    }

    void render(std::int16_t* out, std::size_t frames) override {
        if (!out || frames == 0) return;
        std::lock_guard<std::mutex> lock(mu_);
        if (!rate_) {
            std::memset(out, 0, frames * 2 * sizeof(std::int16_t));
            return;
        }
        // The chip's 16-bit stereo as it comes, resampled by Nuked from 49,716 Hz to the rate.
        // [DESIGN] Not gated when nothing sounds: a released operator at full attenuation
        // still outputs -1 in the negative half of its wave [REF Nuked OPL3 opl3.c,
        // OPL3_EnvelopeCalcSin0], so a paused or finished track leaves a floor of a few LSB
        // (about -68 dBFS peak, at the released notes' pitches) as the chip does; the player
        // keeps rendering while paused for the release tails (M7 s2b fixer).
        constexpr std::size_t kChunk = 1u << 20;
        while (frames) {
            const std::size_t n = frames < kChunk ? frames : kChunk;
            OPL3_GenerateStream(&chip_, out, static_cast<std::uint32_t>(n));
            out += n * 2;
            frames -= n;
        }
    }

    std::string name() const override { return "opl3"; }

private:
    // The chip's power-on state at the rate, then the driver's start state.
    void reset_locked() {
        OPL3_Reset(&chip_, rate_);
        direct_ = true;
        driver_.reset();
        direct_ = false;
    }

    std::mutex mu_;
    opl3_chip chip_;
    bool direct_ = false;
    std::uint32_t rate_ = 0;  // 0 until open
    Opl3Driver driver_;
};

}  // namespace

std::unique_ptr<Synth> make_opl3() { return std::make_unique<Opl3Synth>(); }

}  // namespace is::audio::music

#else  // IS_OPL3 off

namespace is::audio::music {

// IS_OPL3 is off: not built in, as audio/synth.h allows. [DESIGN]
std::unique_ptr<Synth> make_opl3() {
    static_assert(!kOpl3BuiltIn, "an OPL3 build needs the synth above");
    return nullptr;
}

}  // namespace is::audio::music

#endif
