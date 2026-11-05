#pragma once

#include <algorithm>
#include <cmath>

namespace voxel {

// Both timestamps must use the same monotonic clock. Include rendering and
// buffer swap in the measured interval so VSync and an explicit cap do not
// each add a full frame's delay. Every frame starts a fresh budget: missed
// deadlines never accumulate into catch-up bursts after a slow frame.
inline double frameSleepSeconds(double frameStartSeconds, double nowSeconds, int fpsLimit)
{
    if (fpsLimit <= 0 || !std::isfinite(frameStartSeconds) || !std::isfinite(nowSeconds)
        || nowSeconds < frameStartSeconds) return 0;
    return std::max(0.0, 1.0 / fpsLimit - (nowSeconds - frameStartSeconds));
}

} // namespace voxel
