#pragma once

namespace voxel {

// Requires an active OpenGL context. Checks native output/depth, opaque water
// snapshots, setting changes, and renderer image stability at lower resolution.
void verifyRenderScale();

} // namespace voxel
