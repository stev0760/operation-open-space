// The frame scheduler of ADR 0001: per-state paces, frame spacing, no catch-up and the
// restart rule for a change of FrameRateSet.
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "shell/frame_scheduler.h"

using namespace is::shell;

namespace {

// Runs `n` frames on time (each at its due tick) and returns the gaps between them.
std::vector<Tick> spacing(FrameScheduler& s, Tick start, int n) {
    std::vector<Tick> gaps;
    Tick now = start;
    Tick last = 0;
    for (int i = 0; i < n; ++i) {
        now += s.wait(now);
        REQUIRE(s.due(now));
        if (i > 0) gaps.push_back(now - last);
        last = now;
        s.ran(now);
    }
    return gaps;
}

}  // namespace

TEST_CASE("scheduler: the pace of each screen state") {
    CHECK(pace_for_state(0) == Pace::kIntro);
    CHECK(pace_for_state(1) == Pace::kWave);
    for (int s : {2, 3, 5, 7, 8, 9}) CHECK(pace_for_state(s) == Pace::kScreen);
    CHECK(pace_for_state(4) == Pace::kNone);
    CHECK(pace_for_state(6) == Pace::kScreen);
    CHECK(pace_for_state(6, {false, true}) == Pace::kScreen32);
    CHECK(pace_for_state(6, {true, true}) == Pace::kWave);
    CHECK(pace_for_state(10) == Pace::kWave);  // outside 0-9: state 1's case
    CHECK(pace_for_state(-1) == Pace::kWave);
}

TEST_CASE("scheduler: R = 36 spaces wave frames 27-28 ms apart, 36 per second") {
    FrameScheduler s;
    s.set_pace(Pace::kWave, 1000);
    CHECK(s.rate() == 36);
    const auto gaps = spacing(s, 1000, 37);
    Tick total = 0;
    for (Tick g : gaps) {
        CHECK((g == 27 || g == 28));
        total += g;
    }
    CHECK(total == 1000);  // frames 0 and 36 are exactly one second apart
}

TEST_CASE("scheduler: R = 1200 runs frames back to back") {
    FrameScheduler s;
    s.set_pace(Pace::kWave, 50);
    s.set_rate(1200, 50);
    for (Tick g : spacing(s, 50, 20)) CHECK(g == 0);
    CHECK(s.due(50));
    CHECK(s.wait(50) == 0);
}

TEST_CASE("scheduler: the intro runs every 13 ms, the screens at 72 Hz, state 4 not at all") {
    FrameScheduler s;
    s.set_pace(Pace::kIntro, 0);
    for (Tick g : spacing(s, 0, 30)) CHECK(g == 13);

    s.set_pace(Pace::kScreen, 5000);
    const auto gaps = spacing(s, 5000, 73);
    Tick total = 0;
    for (Tick g : gaps) {
        CHECK((g == 13 || g == 14));
        total += g;
    }
    CHECK(total == 1000);

    s.set_pace(Pace::kScreen32, 9000);
    for (Tick g : spacing(s, 9000, 10)) CHECK((g == 31 || g == 32));

    s.set_pace(Pace::kNone, 12000);
    CHECK_FALSE(s.due(12000));
    CHECK_FALSE(s.due(99999));
    CHECK(s.wait(12000) == kIdlePassMs);
}

TEST_CASE("scheduler: a change of pace starts with a frame due at once") {
    FrameScheduler s;
    s.set_pace(Pace::kWave, 100);
    CHECK(s.due(100));
    s.ran(100);
    CHECK_FALSE(s.due(101));
    s.set_pace(Pace::kScreen, 101);
    CHECK(s.due(101));
    // the same pace keeps the schedule
    s.ran(101);
    s.set_pace(Pace::kScreen, 102);
    CHECK_FALSE(s.due(102));
}

TEST_CASE("scheduler: a change of R restarts the schedule from the current frame") {
    FrameScheduler s;
    s.set_pace(Pace::kWave, 0);
    spacing(s, 0, 10);  // frames at 0, 27, 55, ..., 250 (frame 9)
    const Tick last = 250;
    CHECK(s.next_due() == 277);
    s.set_rate(72, last + 5);  // a speed key during the pass after frame 9
    CHECK(s.rate() == 72);
    CHECK(s.next_due() == last + 13);  // one new interval after the last frame
    CHECK(spacing(s, last + 5, 3) == std::vector<Tick>{14, 14});  // 13.9: 263, 277, 291
    // the same rate changes nothing
    const Tick due = s.next_due();
    s.set_rate(72, due - 1);
    CHECK(s.next_due() == due);
    // before any frame has run the schedule starts at `now`
    FrameScheduler t;
    t.set_pace(Pace::kWave, 40);
    t.set_rate(54, 41);
    CHECK(t.next_due() == 41);
    // a rate below 1 counts as 1
    t.set_rate(0, 41);
    CHECK(t.rate() == 1);
}

TEST_CASE("scheduler: a late frame runs once and restarts the schedule (no catch-up)") {
    FrameScheduler s;
    s.set_pace(Pace::kWave, 0);
    s.ran(0);  // frame 0 at 0; frame 1 due at 27
    // a stall: the next frame runs at 500, long after frames 1..17 were due
    CHECK(s.due(500));
    s.ran(500);
    CHECK_FALSE(s.due(500));  // only one frame for the whole stall
    CHECK(s.next_due() == 527);
    // jitter below one interval keeps the schedule: frame due at 527 runs at 530
    s.ran(530);
    CHECK(s.next_due() == 555);
}

TEST_CASE("scheduler: ticks compare modulo 2^32") {
    CHECK(tick_reached(5, 0xFFFFFFF0u));
    CHECK_FALSE(tick_reached(0xFFFFFFF0u, 5));
    FrameScheduler s;
    s.set_pace(Pace::kIntro, 0xFFFFFFF8u);
    s.ran(0xFFFFFFF8u);
    CHECK(s.next_due() == 5u);
    CHECK_FALSE(s.due(0xFFFFFFFFu));
    CHECK(s.wait(0xFFFFFFFFu) == 6u);
    CHECK(s.due(5));
}

TEST_CASE("scheduler: R = 1200 stays due past 2^31 ms and across the 32-bit wrap") {
    FrameScheduler s;
    s.set_pace(Pace::kWave, 0);
    s.set_rate(1200, 0);
    for (Tick t = 0; t < 100; ++t) s.ran(t);
    CHECK(s.due(0x7fffffffu));
    CHECK(s.due(0x80000000u));  // the origin follows the frames; it stays due
    CHECK(s.wait(0x80000000u) == 0u);
    s.ran(0x80000000u);
    // frames keep running through the wrap of the tick
    for (Tick t = 0xFFFFFFF0u; t != 0x10u; ++t) {
        CHECK(s.due(t));
        s.ran(t);
    }
    CHECK(s.due(0x10u));
}
