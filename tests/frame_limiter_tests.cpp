#include "graphics/frame_limiter.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void check(bool passed, const char* message)
{
    if (!passed) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool close(double a, double b) { return std::abs(a - b) < 1e-9; }
} // namespace

int main()
{
    using voxel::frameSleepSeconds;
    check(frameSleepSeconds(10, 10, 0) == 0, "unlimited mode never sleeps");
    check(frameSleepSeconds(10, 10, -60) == 0, "invalid limits cannot produce a negative delay");
    for (const int limit : {30, 60, 90, 120, 144, 240}) {
        const double budget = 1.0 / limit;
        check(close(frameSleepSeconds(10, 10, limit), budget), "an empty frame receives the full cap interval");
        check(close(frameSleepSeconds(10, 10 + budget * .4, limit), budget * .6),
              "rendering work is subtracted from the wait");
        check(close(frameSleepSeconds(10, 10 + budget, limit), 0),
              "a VSync swap already consuming the interval adds no extra wait");
        check(frameSleepSeconds(10, 10 + budget * 3, limit) == 0,
              "slow frames do not wait after missing their deadline");
        check(close(frameSleepSeconds(12, 12 + budget * .2, limit), budget * .8),
              "a later fast frame uses its own budget after the overrun");
    }
    check(frameSleepSeconds(11, 10, 60) == 0, "a backwards clock cannot cause a long sleep");
    const auto infinity = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    check(frameSleepSeconds(10, infinity, 60) == 0 && frameSleepSeconds(-infinity, 10, 60) == 0,
          "infinite timestamps are ignored");
    check(frameSleepSeconds(nan, 10, 60) == 0 && frameSleepSeconds(10, nan, 60) == 0,
          "invalid timestamps are ignored");
    return failures ? 1 : 0;
}
