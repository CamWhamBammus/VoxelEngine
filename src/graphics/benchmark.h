#pragma once

namespace voxel {
struct Options;
// Fixed, warmed native views; does not load or modify player progress.
int runRenderBenchmark(const Options& options);
}
