// The wave start's extra repaint decision (src/hud/wave_start_repaint.h, ADR 0013 decision 3).
#include "doctest.h"

#include "hud/wave_start_repaint.h"

using is::hud::WaveStartRepaint;

TEST_CASE("wave start repaint: the post's own creation in state 1 or 7 repaints once") {
    WaveStartRepaint r;
    CHECK_FALSE(r.take(true, is::shell::kWave));  // no post: the Alt+I creation keeps the erase
    r.posted();
    CHECK(r.due());
    CHECK(r.take(true, is::shell::kWave));
    CHECK_FALSE(r.take(true, is::shell::kWave));  // taken
    r.posted();
    CHECK(r.take(true, is::shell::kHelp));
}

TEST_CASE("wave start repaint: no window, another state, or a destroy drops the mark") {
    WaveStartRepaint r;
    r.posted();
    CHECK_FALSE(r.take(false, is::shell::kWave));
    CHECK_FALSE(r.take(true, is::shell::kWave));  // the failed creation took it
    r.posted();
    CHECK_FALSE(r.take(true, is::shell::kBriefing));
    r.posted();
    r.drop();
    CHECK_FALSE(r.due());
    CHECK_FALSE(r.take(true, is::shell::kWave));
}
