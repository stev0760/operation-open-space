#include "shell/frame_scheduler.h"

namespace is::shell {

Pace pace_for_state(int state, const PaceInputs& in) {
    switch (state) {
    case 0: return Pace::kIntro;
    case 2:
    case 3:
    case 5:
    case 7:  // the dialog pass seg17:0x60d8 [BYTES seg2:0x1cdb]
    case 8:
    case 9: return Pace::kScreen;
    case 4: return Pace::kNone;  // no frame function [BYTES seg7:0x3f1]
    case 6:
        // The gate holds the frames S ... F + 1 ms apart (decision 2) [BYTES seg7:0x126-0x163];
        // otherwise seg7:0x2206's targets for every phase (R1) [BYTES seg7:0x2241-0x2267].
        if (in.state6_gate) return Pace::kWave;
        return in.state6_slow ? Pace::kScreen32 : Pace::kScreen;
    default: return Pace::kWave;  // state 1 and values outside 0-9 [BYTES seg7:0x492]
    }
}

Tick FrameScheduler::offset(std::uint64_t k) const {
    switch (pace_) {
    case Pace::kNone: return 0;
    case Pace::kIntro: return static_cast<Tick>(k * kIntroIntervalMs);
    case Pace::kScreen: return static_cast<Tick>(k * 1000 / kScreenRate);
    case Pace::kScreen32: return static_cast<Tick>(k * 1000 / kScreenSlowRate);
    case Pace::kWave:
        if (rate_ > kMaxRateThreshold) return 0;  // back to back (decision 6)
        return static_cast<Tick>(k * 1000 / static_cast<std::uint64_t>(rate_));
    }
    return 0;
}

void FrameScheduler::restart(Tick now) {
    origin_ = now;
    k_ = 0;
}

void FrameScheduler::set_pace(Pace pace, Tick now) {
    if (pace == pace_) return;
    const bool screens = (pace_ == Pace::kScreen || pace_ == Pace::kScreen32) &&
                         (pace == Pace::kScreen || pace == Pace::kScreen32);
    pace_ = pace;
    // State 6's 72 <-> 32 Hz change: seg7:0x2206 changes only its target and keeps the threshold
    // DGROUP:0x9004, so no frame runs early; the next frame is due one new interval after the last
    // one, or at once when that has passed [BYTES seg7:0x2213-0x2267] [DESIGN: ADR 0001 decision 8]
    // (review L7).
    if (screens && has_run_ && !tick_reached(now, last_run_ + offset(1))) {
        origin_ = last_run_;
        k_ = 1;
        return;
    }
    has_run_ = false;
    restart(now);
}

void FrameScheduler::set_rate(int rate, Tick now) {
    if (rate < 1) rate = 1;
    if (rate == rate_) return;
    rate_ = rate;
    if (pace_ != Pace::kWave) return;
    if (has_run_) {
        origin_ = last_run_;
        k_ = 1;
    } else {
        restart(now);
    }
}

bool FrameScheduler::due(Tick now) const {
    if (pace_ == Pace::kNone) return false;
    return tick_reached(now, offset_due(k_));
}

Tick FrameScheduler::wait(Tick now) const {
    if (pace_ == Pace::kNone) return kIdlePassMs;
    const Tick at = offset_due(k_);
    return tick_reached(now, at) ? 0 : at - now;
}

void FrameScheduler::ran(Tick now) {
    has_run_ = true;
    last_run_ = now;
    ++k_;
    if (offset(1) == 0) {
        // Back to back (decision 6): every frame is due at once. The origin follows the last
        // frame, so `now - origin_` stays small and tick_reached keeps holding after 2^31 ms.
        origin_ = now;
        k_ = 0;
        return;
    }
    if (tick_reached(now, offset_due(k_))) {
        // Late by a whole interval: restart from this frame (decision 3).
        origin_ = now;
        k_ = 1;
    }
    // Keep k small: rebase once the offsets would grow large [DESIGN].
    if (k_ >= (1u << 20)) {
        origin_ = offset_due(k_ - 1);
        k_ = 1;
        // offset(1) from the rebased origin can differ from offset(k) - offset(k-1) by at most
        // 1 ms; the drift is bounded and far below one frame.
    }
}

}  // namespace is::shell
