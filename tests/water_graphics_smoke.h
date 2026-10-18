#pragma once

namespace voxel {

// Requires a current OpenGL context. Throws if water loses its scene capture,
// depth occlusion, animation, or deterministic cache/resize behavior.
void verifyWaterRendering(int width, int height);

} // namespace voxel
