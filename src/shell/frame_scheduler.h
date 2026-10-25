// The frame schedule of ADR 0001 (docs/decisions/0001-timestep.md): discrete frames on a
// millisecond clock, with the thread sleeping between them (decision 1). The original's pass
// machinery (counter, adaptive thresholds, millisecond gates) is not emulated (decision 7);
// each screen state gets the rate that machinery aims at (decisions 2 and 8). Portable.
//
// Ticks are 32-bit milliseconds, the equivalent of GetCurrentTime (decision 5). Frame k of a
// schedule is due at origin + offset(k), offset(k) = k * 1000 / rate truncated, so a 36 Hz
// schedule spaces its frames 27 or 28 ms apart and averages 36 per second.
#pragma once

#include <cstdint>

namespace is::shell {

using Tick = std::uint32_t;

// True when `now` is at or after `due` in modular 32-bit arithmetic (the schedule's own
// deadlines, which never lie more than 2^31 ms apart) [DESIGN].
constexpr bool tick_reached(Tick now, Tick due) {
    return static_cast<std::int32_t>(now - due) >= 0;
}

// How a state paces its frames.
enum class Pace : std::uint8_t {
    kNone,      // no frame function (state 4): passes only check deadlines
    kIntro,     // state 0: every 13 ms, the lower bound of its 14/13 ms gate (decision 8)
    kScreen,    // 72 Hz, seg7:0x2206's usual target (decision 8)
    kScreen32,  // 32 Hz, seg7:0x2206's state-6 target while DGROUP:0x914a = 1, DGROUP:0x92a2 = 0
    kWave,      // 1000 / FrameRateSet ms (decision 2)
};

// The inputs that pick state 6's pace; the other states ignore them.
struct PaceInputs {
    // DGROUP:0x952c set, DGROUP:0x9530 = 1: the gate condition of timing.md "The frame gate".
    bool state6_gate = false;
    // DGROUP:0x914a = 1 and DGROUP:0x92a2 = 0 (timing.md "Adaptive screen threshold").
    bool state6_slow = false;
};

// The pace of a screen state (docs/re/shell.md "Screen states"): state 1 the wave rate;
// state 0 the intro gate; states 2, 3, 5, 8, 9 and the help pass of state 7 72 Hz; state 6
// 72 Hz (32 Hz in its slow phase), or the wave rate under its gate; state 4 none. Values
// outside 0-9 share state 1's tick case [BYTES seg7:0x492] and so its pace.
Pace pace_for_state(int state, const PaceInputs& in = {});

// FrameRateSet above this runs frames back to back (decision 6) [BYTES seg7:0x9ed].
constexpr int kMaxRateThreshold = 1000;
// FrameRateSet before the INI is read: the initialised DGROUP:0x59c [BYTES DGROUP:0x59c].
constexpr int kDefaultFrameRate = 36;
// The intro's spacing in ms (decision 8) and seg7:0x2206's targets [BYTES seg7:0x2262].
constexpr int kIntroIntervalMs = 13;
constexpr int kScreenRate = 72;
constexpr int kScreenSlowRate = 32;
// How long a state without frames (Pace::kNone) waits between passes, so that its
// wall-clock deadlines are seen within this many ms [DESIGN].
constexpr Tick kIdlePassMs = 10;

class FrameScheduler {
public:
    // A schedule with no pace; set_pace starts one.
    FrameScheduler() = default;

    Pace pace() const { return pace_; }
    int rate() const { return rate_; }

    // Switches the pace. A change restarts the schedule with its first frame due at `now`;
    // the same pace keeps the schedule.
    void set_pace(Pace pace, Tick now);
    // Sets FrameRateSet (values below 1 count as 1 [DESIGN]: the original divides by it). A
    // change of R restarts the schedule from the current frame (decision 2): the next frame
    // is due one new interval after the last frame that ran, or at `now` when none has run.
    // The rate is kept for the wave pace even while another pace is active.
    void set_rate(int rate, Tick now);
    // Starts the schedule afresh: frame 0 due at `now`.
    void restart(Tick now);

    // The next frame is due.
    bool due(Tick now) const;
    // Milliseconds until the next frame is due, 0 when it is due. Pace::kNone gives
    // kIdlePassMs.
    Tick wait(Tick now) const;
    // The tick at which the next frame is due (meaningless for Pace::kNone).
    Tick next_due() const { return offset_due(k_); }

    // Call after running a frame at `now`. Without catch-up (decision 3): when the frame
    // after it is already due, the port is late, and the schedule restarts from this frame.
    // A frame that runs late by less than one interval keeps the schedule, so wake-up jitter
    // does not lower the average rate [DESIGN].
    void ran(Tick now);

    // The offset of frame k from the origin, in ms, for the current pace and rate.
    Tick offset(std::uint64_t k) const;

private:
    Tick offset_due(std::uint64_t k) const { return origin_ + offset(k); }

    Pace pace_ = Pace::kNone;
    int rate_ = kDefaultFrameRate;
    Tick origin_ = 0;
    std::uint64_t k_ = 0;
    bool has_run_ = false;
    Tick last_run_ = 0;
};

}  // namespace is::shell
